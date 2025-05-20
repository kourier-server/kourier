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

#ifndef KOURIER_TIMER_H
#define KOURIER_TIMER_H

#include "Object.h"


namespace Kourier
{
class TimerPrivate;

class KOURIER_EXPORT Timer : public Object
{
KOURIER_OBJECT(Kourier::Timer)
public:
    Timer();
    ~Timer() override;
    void start();
    void start(qint64 intervalInMSecs);
    void stop();
    Signal timeout();
    bool isActive() const;
    bool isSingleShot() const;
    void setSingleShot(bool singleShot);
    qint64 interval() const;
    void setInterval(qint64 intervalInMSecs);

private:
    TimerPrivate *d_ptr;
    Q_DECLARE_PRIVATE(Timer)
};

}

#endif // KOURIER_TIMER_H
