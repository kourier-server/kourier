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
#include <chrono>


namespace Kourier
{

class TimerPrivate
{
public:
    TimerPrivate();
    ~TimerPrivate() {stop();}
    inline void start() {activateTimer(m_interval);}
    inline void start(std::chrono::nanoseconds interval) {activateTimer(interval);}
    inline void stop() {deactivateTimer();}
    inline bool isActive() const {return m_state == State::Active;}
    inline bool isSingleShot() const {return m_isSingleShot;}
    inline void setSingleShot(bool singleShot) {m_isSingleShot = singleShot;}
    inline std::chrono::nanoseconds interval() const {return m_interval;}
    void setInterval(std::chrono::nanoseconds interval);

private:
    void activateTimer(std::chrono::nanoseconds interval);
    void deactivateTimer();
    void processTimeout();

private:
    Timer *q_ptr = nullptr;
    Q_DECLARE_PUBLIC(Timer)
    std::chrono::nanoseconds m_interval;
    EpollEventNotifier * const m_pEventNotifier;
    bool m_isSingleShot = false;
    enum class State : uint8_t {Active, Inactive};
    State m_state = State::Inactive;
    friend class EpollTimerRegistrar;
    friend class TimerWheel;
};

}

#endif // KOURIER_TIMER_PRIVATE_H
