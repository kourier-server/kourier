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

#ifndef KOURIER_NO_DESTROY_H
#define KOURIER_NO_DESTROY_H

#include <utility>


namespace Kourier
{

template <class T>
class NoDestroy
{
public:
    template <class... Args>
    constexpr NoDestroy(Args... args) : m_object(std::forward<Args...>(args)...) {}
    ~NoDestroy() {}
    inline T &operator()() {return m_object;}
    inline const T &operator()() const {return m_object;}

private:
    union {T m_object;};
};

template <class T>
class NoDestroyPtrDeleter
{
    static_assert(std::is_pointer_v<T>);
public:
    NoDestroyPtrDeleter(NoDestroy<T> &var) : m_var(var) {}
    ~NoDestroyPtrDeleter()
    {
        if (m_var())
        {
            delete m_var();
            m_var() = nullptr;
        }
    }

private:
    NoDestroy<T> &m_var;
};

template <class T>
class NoDestroyCleaner
{
    static_assert(!std::is_pointer_v<T>);
    typedef void (T::*T_Cleaner)();
public:
    NoDestroyCleaner(NoDestroy<T> &var, T_Cleaner pCleanerFcn) :
        m_var(var),
        m_pCleanerFcn(pCleanerFcn)
    {
        assert(m_pCleanerFcn != nullptr);
    }
    ~NoDestroyCleaner() {(m_var().*m_pCleanerFcn)();}

private:
    NoDestroy<T> &m_var;
    T_Cleaner m_pCleanerFcn;
};

}

#endif // KOURIER_NO_DESTROY_H
