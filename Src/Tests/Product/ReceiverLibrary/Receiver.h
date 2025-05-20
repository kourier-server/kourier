//
// Copyright (C) 2025 Glauco Pacheco <glauco@kourier.io>
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

#ifndef KOURIER_SPEC_PRODUCT_RECEIVER_H
#define KOURIER_SPEC_PRODUCT_RECEIVER_H

#include <Kourier>
#include <QtCompilerDetection>

#if defined(KOURIER_EXPORT_RECEIVER_LIBRARY)
#define KOURIER_RECEIVER_EXPORT Q_DECL_EXPORT
#else
#define KOURIER_RECEIVER_EXPORT Q_DECL_IMPORT
#endif

using Kourier::Object;


class KOURIER_RECEIVER_EXPORT Receiver : public Object
{
    KOURIER_OBJECT(Receiver)
public:
    Receiver() = default;
    ~Receiver() override = default;
    void onValueChanged(int value);
    int value() const;

private:
    int m_value = 0;
};

#endif // KOURIER_SPEC_PRODUCT_RECEIVER_H
