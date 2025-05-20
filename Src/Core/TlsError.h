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

#ifndef KOURIER_TLS_ERROR_H
#define KOURIER_TLS_ERROR_H


namespace Kourier
{

class TlsError
{
public:
    TlsError() = default;
    ~TlsError() = default;
    static TlsError getError();

private:
    std::string_view m_errorLibName;
    std::string_view m_errorReason;
    unsigned long m_errorCode = 0;
};

}

#endif // KOURIER_TLS_ERROR_H
