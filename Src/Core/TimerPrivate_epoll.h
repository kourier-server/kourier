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

#ifndef KOURIER_TIMER_PRIVATE_H
#define KOURIER_TIMER_PRIVATE_H

#include "Timer.h"


namespace Kourier
{

class TimerPrivate
{
public:
    TimerPrivate(Timer *pTimer);
    ~TimerPrivate() {stop();}
    inline void start() {activateTimer(m_intervalInMSecs);}
    inline void start(qint64 intervalInMSecs) {activateTimer(intervalInMSecs);}
    inline void stop() {deactivateTimer();}
    inline bool isActive() const {return m_state == State::Active;}
    inline bool isSingleShot() const {return m_isSingleShot;}
    inline void setSingleShot(bool singleShot) {m_isSingleShot = singleShot;}
    inline qint64 interval() const {return m_intervalInMSecs;}
    void setInterval(qint64 intervalInMSecs);

private:
    void activateTimer(qint64 intervalInMSecs);
    void deactivateTimer();
    void processTimeout();
    inline qint64 timeoutInSlices() const {return m_timeoutInSlices;}
    void setTimeoutInSlices(qint64 timeoutInSlices) {m_timeoutInSlices = timeoutInSlices;}

private:
    Timer * const q_ptr;
    Q_DECLARE_PUBLIC(Timer)
    qint64 m_intervalInMSecs = 0;
    qint64 m_timeoutInSlices = 0;
    EpollEventNotifier * const m_pEventNotifier;
    TimerPrivate *m_pNext = nullptr;
    TimerPrivate *m_pPrevious = nullptr;
    bool m_isSingleShot = false;
    enum class State : uint8_t {Active, Inactive};
    State m_state = State::Inactive;
    enum class TimerType : uint8_t {VeryCoarse, Coarse, Precise};
    const TimerType m_timerType = TimerType::VeryCoarse;
    friend class EpollTimerRegistrar;
};

}

#endif // KOURIER_TIMER_PRIVATE_H
