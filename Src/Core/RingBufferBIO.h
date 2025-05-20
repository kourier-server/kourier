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

#ifndef KOURIER_RING_BUFFER_BIO_H
#define KOURIER_RING_BUFFER_BIO_H

#include "RingBuffer.h"
#include <openssl/bio.h>


namespace Kourier
{

class RingBufferBIO
{
public:
    RingBufferBIO();
    ~RingBufferBIO();
    inline BIO *bio() {return m_pBio;}
    inline RingBuffer &ringBuffer() {return *m_pRingBuffer;}

private:
    static int bioWriteEx(BIO *pBio, const char *pData, size_t size, size_t *pBytesWritten);
    static int bioWrite(BIO *pBio, const char *pData, int size);
    static int bioReadEx(BIO *pBio, char *pBuffer, size_t size, size_t *pBytesRead);
    static int bioRead(BIO *pBio, char *pBuffer, int size);
    static int bioPuts(BIO *pBio, const char *str);
    static int bioGets(BIO *pBio, char *str, int size);
    static long bioCtrl(BIO *pBio, int cmd, long arg1, void *arg2);
    static int bioNew(BIO *pBio);
    static int bioDelete(BIO *pBio);

private:
    BIO *m_pBio = nullptr;
    RingBuffer *m_pRingBuffer = nullptr;
};

}

#endif // KOURIER_RING_BUFFER_BIO_H
