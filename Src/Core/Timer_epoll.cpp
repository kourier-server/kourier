//
// Copyright (C) 2023 Glauco Pacheco <glauco@kourier.io>
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

#include "Timer.h"
#include "TimerPrivate_epoll.h"


namespace Kourier
{

Timer::Timer()
{
    d_ptr = new TimerPrivate(this);
}

Timer::~Timer()
{
    delete d_ptr;
}

void Timer::start()
{
    Q_D(Timer);
    d->start();
}

void Timer::start(qint64 intervalInMSecs)
{
    Q_D(Timer);
    d->start(intervalInMSecs);
}

void Timer::stop()
{
    Q_D(Timer);
    d->stop();
}

Signal Timer::timeout() KOURIER_SIGNAL(&Timer::timeout)

bool Timer::isActive() const
{
    Q_D(const Timer);
    return d->isActive();
}

bool Timer::isSingleShot() const
{
    Q_D(const Timer);
    return d->isSingleShot();
}

void Timer::setSingleShot(bool singleShot)
{
    Q_D(Timer);
    return d->setSingleShot(singleShot);
}

qint64 Timer::interval() const
{
    Q_D(const Timer);
    return d->interval();
}

void Timer::setInterval(qint64 intervalInMSecs)
{
    Q_D(Timer);
    return d->setInterval(intervalInMSecs);
}

}
