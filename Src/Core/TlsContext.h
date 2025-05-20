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

#ifndef KOURIER_TLS_CONTEXT_H
#define KOURIER_TLS_CONTEXT_H

#include "TlsConfiguration.h"
#include "RuntimeError.h"
#include <openssl/ssl.h>
#include <memory>
#include <utility>


namespace Kourier
{

class TlsContext
{
public:
    enum class Role {Client, Server};
    TlsContext() = default;
    TlsContext(Role role, const TlsConfiguration &tlsConfiguration);
    TlsContext(const TlsContext&) = default;
    ~TlsContext() = default;
    TlsContext& operator=(const TlsContext&) = default;
    inline SSL_CTX *context() const {assert(m_pTlsContextData); return m_pTlsContextData->m_pContext;}
    const TlsConfiguration &tlsConfiguration() const {assert(m_pTlsContextData); return m_pTlsContextData->m_tlsConfiguration;}
    Role role() const {assert(m_pTlsContextData); return m_pTlsContextData->m_role;}
    static TlsContext fromTlsConfiguration(const TlsConfiguration &tlsConfiguration, Role role);
    static std::pair<bool, std::string> validateTlsConfiguration(const TlsConfiguration &tlsConfiguration, Role role);

private:
    struct TlsContextData
    {
        TlsContextData() = default;
        TlsContextData(const TlsConfiguration &tlsConfiguration, Role role) :
            m_tlsConfiguration(tlsConfiguration),
            m_role(role)
        {
            auto *pContext = SSL_CTX_new((role == Role::Client) ? TLS_client_method() : TLS_server_method());
            if (pContext == nullptr)
                throw RuntimeError("Failed to create OpenSSL context.", RuntimeError::ErrorType::TLS);
            m_pContext = pContext;
        }
        ~TlsContextData() {SSL_CTX_free(m_pContext);}
        SSL_CTX *m_pContext = nullptr;
        TlsConfiguration m_tlsConfiguration;
        Role m_role = Role::Client;
    };
    std::shared_ptr<TlsContextData> m_pTlsContextData;
};

}

#endif // KOURIER_TLS_CONTEXT_H
