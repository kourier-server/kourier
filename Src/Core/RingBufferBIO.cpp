//
// Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
// SPDX-License-Identifier: AGPL-3.0-only
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "RingBufferBIO.h"
#include "NoDestroy.h"
#include "RuntimeError.h"


namespace Kourier
{

RingBufferBIO::RingBufferBIO()
{
    const static NoDestroy<BIO_METHOD*> m_pBioMethod = []() -> BIO_METHOD*
    {
        auto * const pBioMethod = BIO_meth_new(BIO_get_new_index() | BIO_TYPE_SOURCE_SINK, "BIO_kourier");
        if (pBioMethod == nullptr)
            qFatal("%s", RuntimeError("Failed to allocate OpenSSL BIO_METHOD.", RuntimeError::ErrorType::TLS).error().data());
        if (BIO_meth_set_write_ex(pBioMethod, &RingBufferBIO::bioWriteEx) != 1)
            qFatal("%s", RuntimeError("Failed to set method for OpenSSL BIO_write_ex.", RuntimeError::ErrorType::TLS).error().data());
        if (BIO_meth_set_write(pBioMethod, &RingBufferBIO::bioWrite) != 1)
            qFatal("%s", RuntimeError("Failed to set method for OpenSSL BIO_write.", RuntimeError::ErrorType::TLS).error().data());
        if (BIO_meth_set_read_ex(pBioMethod, &RingBufferBIO::bioReadEx) != 1)
            qFatal("%s", RuntimeError("Failed to set method for OpenSSL BIO_read_ex.", RuntimeError::ErrorType::TLS).error().data());
        if (BIO_meth_set_read(pBioMethod, &RingBufferBIO::bioRead) != 1)
            qFatal("%s", RuntimeError("Failed to set method for OpenSSL BIO_read.", RuntimeError::ErrorType::TLS).error().data());
        if (BIO_meth_set_puts(pBioMethod, &RingBufferBIO::bioPuts) != 1)
            qFatal("%s", RuntimeError("Failed to set method for OpenSSL BIO_puts.", RuntimeError::ErrorType::TLS).error().data());
        if (BIO_meth_set_gets(pBioMethod, &RingBufferBIO::bioGets) != 1)
            qFatal("%s", RuntimeError("Failed to set method for OpenSSL BIO_gets.", RuntimeError::ErrorType::TLS).error().data());
        if (BIO_meth_set_ctrl(pBioMethod, &RingBufferBIO::bioCtrl) != 1)
            qFatal("%s", RuntimeError("Failed to set method for OpenSSL BIO_ctrl.", RuntimeError::ErrorType::TLS).error().data());
        if (BIO_meth_set_create(pBioMethod, &RingBufferBIO::bioNew) != 1)
            qFatal("%s", RuntimeError("Failed to set method for OpenSSL BIO_new.", RuntimeError::ErrorType::TLS).error().data());
        if (BIO_meth_set_destroy(pBioMethod, &RingBufferBIO::bioDelete) != 1)
            qFatal("%s", RuntimeError("Failed to set method for OpenSSL BIO_free.", RuntimeError::ErrorType::TLS).error().data());
        return pBioMethod;
    }();
    m_pBio = BIO_new(m_pBioMethod());
    if (m_pBio == nullptr)
        qFatal("%s", RuntimeError("Failed to create OpenSSL BIO.", RuntimeError::ErrorType::TLS).error().data());
    m_pRingBuffer = static_cast<RingBuffer*>(BIO_get_data(m_pBio));
    if (m_pRingBuffer == nullptr)
        qFatal("%s", RuntimeError("Failed to create OpenSSL BIO.", RuntimeError::ErrorType::TLS).error().data());
}

RingBufferBIO::~RingBufferBIO()
{
    BIO_free(m_pBio);
}

int RingBufferBIO::bioWriteEx(BIO *pBio, const char *pData, size_t size, size_t *pBytesWritten)
{
    /*
     * BIO_write_ex() attempts to write dlen bytes from data to BIO b. If successful then the number
     * of bytes written is stored in *written unless written is NULL.
     *
     * BIO_write_ex() returns 1 if no error was encountered writing data, 0 otherwise.
     * Requesting to write 0 bytes is not considered an error.
     */
    if (pBio == nullptr || pData == nullptr)
        return 0;
    auto *pRingBuffer = static_cast<RingBuffer*>(BIO_get_data(pBio));
    if (pRingBuffer == nullptr)
        return 0;
    BIO_clear_retry_flags(pBio);
    const auto bytesWritten = pRingBuffer->write(pData, size);
    if (pBytesWritten)
        *pBytesWritten = bytesWritten;
    return 1;
}

int RingBufferBIO::bioWrite(BIO *pBio, const char *pData, int size)
{
    /*
     * BIO_write() attempts to write len bytes from buf to BIO b.
     *
     * BIO_write() returns -2 if the "write" operation is not implemented by the BIO or -1 on other errors.
     * Otherwise it returns the number of bytes written. This may be 0 if the BIO b is NULL or dlen <= 0.
     */
    if (pBio == nullptr || size <= 0)
        return 0;
    if (pData == nullptr)
        return -1;
    auto *pRingBuffer = static_cast<RingBuffer*>(BIO_get_data(pBio));
    if (pRingBuffer == nullptr)
        return -1;
    BIO_clear_retry_flags(pBio);
    return pRingBuffer->write(pData, size);
}

int RingBufferBIO::bioReadEx(BIO *pBio, char *pBuffer, size_t size, size_t *pBytesRead)
{
    /*
     * BIO_read_ex() attempts to read dlen bytes from BIO b and places the data in data. If any
     * bytes were successfully read then the number of bytes read is stored in *readbytes.
     *
     * BIO_read_ex() returns 1 if data was successfully read, and 0 otherwise.
     */
    if (pBio == nullptr || pBuffer == nullptr)
        return 0;
    BIO_clear_retry_flags(pBio);
    auto *pRingBuffer = static_cast<RingBuffer*>(BIO_get_data(pBio));
    if (pRingBuffer == nullptr)
        return 0;
    const auto bytesRead = pRingBuffer->read(pBuffer, size);
    if (pBytesRead)
        *pBytesRead = bytesRead;
    if (bytesRead == 0)
        BIO_set_retry_read(pBio);
    return 1;
}

int RingBufferBIO::bioRead(BIO *pBio, char *pBuffer, int size)
{
    /*
     * BIO_read() attempts to read len bytes from BIO b and places the data in buf.
     *
     * All other functions return either the amount of data successfully read or written
     * (if the return value is positive) or that no data was successfully read or written if
     * the result is 0 or -1. If the return value is -2 then the operation is not implemented
     * in the specific BIO type.
     */
    if (pBio == nullptr || pBuffer == nullptr)
        return -1;
    BIO_clear_retry_flags(pBio);
    auto *pRingBuffer = static_cast<RingBuffer*>(BIO_get_data(pBio));
    if (pRingBuffer == nullptr)
        return -1;
    if (const auto bytesRead = pRingBuffer->read(pBuffer, size); bytesRead > 0)
        return bytesRead;
    else
    {
        BIO_set_retry_read(pBio);
        return 0;
    }
}

int RingBufferBIO::bioPuts(BIO *pBio, const char *str)
{
    /*
     * BIO_puts() attempts to write a NUL-terminated string buf to BIO b.
     *
     * All other functions return either the amount of data successfully read or written
     * (if the return value is positive) or that no data was successfully read or written
     * if the result is 0 or -1. If the return value is -2 then the operation is not
     * implemented in the specific BIO type.
     */
    if (pBio == nullptr || str == nullptr)
        return -1;
    auto *pRingBuffer = static_cast<RingBuffer*>(BIO_get_data(pBio));
    if (pRingBuffer == nullptr)
        return -1;
    BIO_clear_retry_flags(pBio);
    return pRingBuffer->write(std::string_view{str});
}

int RingBufferBIO::bioGets(BIO *pBio, char *str, int size)
{
    return -2;
}

long RingBufferBIO::bioCtrl(BIO *pBio, int cmd, long arg1, void *arg2)
{
    // Source/sink BIOs return an 0 if they do not recognize the BIO_ctrl() operation.
    if (pBio == nullptr)
        return 0;
    auto *pRingBuffer = static_cast<RingBuffer*>(BIO_get_data(pBio));
    if (pRingBuffer == nullptr)
        return 0;
    long ret = 1;
    switch (cmd)
    {
        case BIO_CTRL_RESET:
            pRingBuffer->clear();
            break;
        case BIO_CTRL_PENDING:
            ret = (long)pRingBuffer->size();
            break;
        case BIO_CTRL_EOF:
            ret = 0;
            break;
        case BIO_CTRL_GET_KTLS_SEND:
        case BIO_CTRL_GET_KTLS_RECV:
            ret = 0;
            break;
        case BIO_CTRL_WPENDING:
            ret = 0L;
            break;
        case BIO_CTRL_DUP:
        case BIO_CTRL_FLUSH:
            ret = 1;
            break;
        case BIO_CTRL_PUSH:
        case BIO_CTRL_POP:
            ret = 0;
            break;
        default:
            qDebug("%s%d%s%ld%s", "BIO_ctrl was called with unrecognized operation ", cmd, " and arg ", arg1, ".");
            ret = 0;
            break;
    }
    return ret;
}

int RingBufferBIO::bioNew(BIO *pBio)
{
    if (pBio == nullptr)
        return 0;
    auto *pRingBuffer = new RingBuffer;
    if (pRingBuffer == nullptr)
        return 0;
    BIO_set_data(pBio, pRingBuffer);
    BIO_set_init(pBio, 1);
    return 1;
}

int RingBufferBIO::bioDelete(BIO *pBio)
{
    if (pBio == nullptr)
        return 0;
    auto *pRingBuffer = static_cast<RingBuffer*>(BIO_get_data(pBio));
    if (pRingBuffer == nullptr)
        return 0;
    delete pRingBuffer;
    BIO_set_data(pBio, nullptr);
    return 1;
}

}
