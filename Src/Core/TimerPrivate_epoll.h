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
#include "TimerListNode.h"
#include <chrono>


namespace Kourier
{

class TimerWheel;

class TimerPrivate
{
public:
    TimerPrivate();
    ~TimerPrivate() {stop();}
    inline void start() {activateTimer(m_interval);}
    inline void start(std::chrono::milliseconds interval) {activateTimer(interval);}
    inline void stop() {deactivateTimer();}
    inline bool isActive() const {return m_state == State::Active;}
    inline bool isSingleShot() const {return m_isSingleShot;}
    inline void setSingleShot(bool singleShot) {m_isSingleShot = singleShot;}
    inline std::chrono::milliseconds interval() const {return m_interval;}
    inline std::chrono::milliseconds timeout() const {return m_timeout;}
    void setInterval(std::chrono::milliseconds interval);

private:
    void activateTimer(std::chrono::milliseconds interval);
    void deactivateTimer();
    void processTimeout();

private:
    Timer *q_ptr = nullptr;
    Q_DECLARE_PUBLIC(Timer)
    TimerListNode m_listNode;
    TimerWheel *m_pTimerWheel = nullptr;
    size_t m_idxTimerWheelSlot = 0;
    std::chrono::milliseconds m_interval = std::chrono::milliseconds(0);
    std::chrono::milliseconds m_timeout = std::chrono::milliseconds(0);
    std::chrono::milliseconds m_activationTime = std::chrono::milliseconds(0);
    EpollEventNotifier * const m_pEventNotifier;
    bool m_isSingleShot = false;
    enum class State : uint8_t {Active, Inactive};
    State m_state = State::Inactive;
    friend class EpollTimerRegistrar;
    friend class TimerWheel;
    friend class TimerList;
    friend class TimerWheels;
};

}

#endif // KOURIER_TIMER_PRIVATE_H
