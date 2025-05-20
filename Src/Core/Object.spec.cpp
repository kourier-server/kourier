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

#include "Object.h"
#include <QObject>
#include <QElapsedTimer>
#include <memory>
#include <QList>
#include <QStringList>
#include <QByteArray>
#include <QString>
#include <Spectator.h>

using Kourier::Object;


namespace Test::Object
{

static int outerCounter = 0;

class TestObject : public Kourier::Object
{
KOURIER_OBJECT(Test::Object::TestObject)
public:
    ~TestObject() override = default;
    int add(const int a, const int b) {return a+b;}
    int value() const {return m_value;}
    void resetValue() {m_value = 0;}
    void increaseOuterCounter() const {++outerCounter;}
    void increaseValue(int value) {m_value += value;}
    void setCounterInt(int newValue) {m_value = newValue;}
    void setCounterConstInt(const int newValue) {m_value = newValue;}
    void setCounterIntRef(int& newValue) {m_value = newValue;}
    void setCounterConstIntRef(const int& newValue) {m_value = newValue;}
    void setCounterIntPtr(int* newValue) {m_value = *newValue;}
    void setCounterIntConstPtr(int*const newValue) {m_value = *newValue;}
    void setCounterConstIntPtr(const int * newValue) {m_value = *newValue;}
    void setCounterConstIntConstPtr(const int * const newValue) {m_value = *newValue;}
    void setCounterIntPtrRef(int*& newValue) {m_value = *newValue;}
    void setCounterIntConstPtrRef(int*const& newValue) {m_value = *newValue;}
    void setCounterConstIntPtrRef(const int *& newValue) {m_value = *newValue;}
    void setCounterConstIntConstPtrRef(const int * const & newValue) {m_value = *newValue;}
    void setValues(int value1, int value2) {m_value1 = value1; m_value2 = value2;}
    int value1() const {return m_value1;}
    int value2() const {return m_value2;}
    void resetIncrement1() {m_increment1 = 0;}
    void increment1() {m_increment1 += 1;}
    int increment1Value() const {return m_increment1;}
    void resetIncrement2() {m_increment2 = 0;}
    void increment2() {m_increment2 += 2;}
    int increment2Value() const {return m_increment2;}

private:
    int m_value = 0;
    int m_value1 = 0;
    int m_value2 = 0;
    int m_increment1 = 0;
    int m_increment2 = 0;
};

class EmitterClass : public Kourier::Object
{
KOURIER_OBJECT(Test::Object::EmitterClass)
public:
    EmitterClass() = default;
    ~EmitterClass() override = default;
    Kourier::Signal signal();
    Kourier::Signal intSignal(int value);
    Kourier::Signal constIntSignal(const int value);
    Kourier::Signal refIntSignal(int &value);
    Kourier::Signal refConstIntSignal(const int &value);
    Kourier::Signal ptrIntSignal(int *pValue);
    Kourier::Signal ptrConstIntSignal(int const *pValue);
    Kourier::Signal constPtrIntSignal(int * const pValue);
    Kourier::Signal constPtrConstIntSignal(int const * const pValue);
    Kourier::Signal refPtrIntSignal(int *&pValue);
    Kourier::Signal refConstPtrIntSignal(int * const &pValue);
    Kourier::Signal refPtrConstIntSignal(int const *&pValue);
    Kourier::Signal refConstPtrConstIntSignal(int const * const &pValue);
    Kourier::Signal twoIntsSignal(int a, int b);
};

Kourier::Signal Test::Object::EmitterClass::signal() KOURIER_SIGNAL(&Test::Object::EmitterClass::signal)
Kourier::Signal Test::Object::EmitterClass::intSignal(int value) KOURIER_SIGNAL(&Test::Object::EmitterClass::intSignal, value)
Kourier::Signal Test::Object::EmitterClass::constIntSignal(const int value) KOURIER_SIGNAL(&Test::Object::EmitterClass::constIntSignal, value)
Kourier::Signal Test::Object::EmitterClass::refIntSignal(int &value) KOURIER_SIGNAL(&Test::Object::EmitterClass::refIntSignal, value)
Kourier::Signal Test::Object::EmitterClass::refConstIntSignal(const int &value) KOURIER_SIGNAL(&Test::Object::EmitterClass::refConstIntSignal, value)
Kourier::Signal Test::Object::EmitterClass::ptrIntSignal(int *pValue) KOURIER_SIGNAL(&Test::Object::EmitterClass::ptrIntSignal, pValue)
Kourier::Signal Test::Object::EmitterClass::ptrConstIntSignal(int const *pValue) KOURIER_SIGNAL(&Test::Object::EmitterClass::ptrConstIntSignal, pValue)
Kourier::Signal Test::Object::EmitterClass::constPtrIntSignal(int * const pValue) KOURIER_SIGNAL(&Test::Object::EmitterClass::constPtrIntSignal, pValue)
Kourier::Signal Test::Object::EmitterClass::constPtrConstIntSignal(int const * const pValue) KOURIER_SIGNAL(&Test::Object::EmitterClass::constPtrConstIntSignal, pValue)
Kourier::Signal Test::Object::EmitterClass::refPtrIntSignal(int *&pValue) KOURIER_SIGNAL(&Test::Object::EmitterClass::refPtrIntSignal, pValue)
Kourier::Signal Test::Object::EmitterClass::refConstPtrIntSignal(int * const &pValue) KOURIER_SIGNAL(&Test::Object::EmitterClass::refConstPtrIntSignal, pValue)
Kourier::Signal Test::Object::EmitterClass::refPtrConstIntSignal(int const *&pValue) KOURIER_SIGNAL(&Test::Object::EmitterClass::refPtrConstIntSignal, pValue)
Kourier::Signal Test::Object::EmitterClass::refConstPtrConstIntSignal(int const * const &pValue) KOURIER_SIGNAL(&Test::Object::EmitterClass::refConstPtrConstIntSignal, pValue)
Kourier::Signal Test::Object::EmitterClass::twoIntsSignal(int a, int b) KOURIER_SIGNAL(&Test::Object::EmitterClass::twoIntsSignal, a, b)

class DerivedEmitterClass : public EmitterClass
{
KOURIER_OBJECT(Test::Object::DerivedEmitterClass)
public:
    DerivedEmitterClass() = default;
    ~DerivedEmitterClass() override = default;
    Kourier::Signal signal();
};

Kourier::Signal Test::Object::DerivedEmitterClass::signal() KOURIER_SIGNAL(&Test::Object::DerivedEmitterClass::signal)

class NotDerivedEmitterClass : public Kourier::Object
{
KOURIER_OBJECT(Test::Object::NotDerivedEmitterClass)
public:
    NotDerivedEmitterClass() = default;
    ~NotDerivedEmitterClass() override = default;
    Kourier::Signal signal();
};

Kourier::Signal Test::Object::NotDerivedEmitterClass::signal() KOURIER_SIGNAL(&Test::Object::NotDerivedEmitterClass::signal)

}

using namespace Test::Object;


SCENARIO("Object supports safe downcast")
{
    GIVEN("a pointer to an EmitterClass instance")
    {
        EmitterClass object;

        WHEN("the instance is cast into a pointer to an Object")
        {
            auto *pCast = object.tryCast<Object*>();

            THEN("the cast succeeds")
            {
                REQUIRE(pCast != nullptr);
            }
        }

        WHEN("the instance is cast into a pointer to an EmitterClass")
        {
            auto *pCast = object.tryCast<EmitterClass*>();

            THEN("the cast succeeds")
            {
                REQUIRE(pCast != nullptr);
            }
        }

        WHEN("the instance is cast into a pointer to a DerivedEmitterClass")
        {
            auto *pCast = object.tryCast<DerivedEmitterClass*>();

            THEN("the cast fails")
            {
                REQUIRE(pCast == nullptr);
            }
        }

        WHEN("the instance is cast into a pointer to a NotDerivedEmitterClass")
        {
            auto *pCast = object.tryCast<NotDerivedEmitterClass*>();

            THEN("the cast fails")
            {
                REQUIRE(pCast == nullptr);
            }
        }
    }

    GIVEN("a pointer to a const EmitterClass instance")
    {
        const EmitterClass object;

        WHEN("the instance is cast into a pointer to an Object")
        {
            auto *pCast = object.tryCast<Object const *>();

            THEN("the cast succeeds")
            {
                REQUIRE(pCast != nullptr);
            }
        }

        WHEN("the instance is cast into a pointer to an EmitterClass")
        {
            auto *pCast = object.tryCast<EmitterClass const*>();

            THEN("the cast succeeds")
            {
                REQUIRE(pCast != nullptr);
            }
        }

        WHEN("the instance is cast into a pointer to a DerivedEmitterClass")
        {
            auto *pCast = object.tryCast<DerivedEmitterClass const*>();

            THEN("the cast fails")
            {
                REQUIRE(pCast == nullptr);
            }
        }

        WHEN("the instance is cast into a pointer to a NotDerivedEmitterClass")
        {
            auto *pCast = object.tryCast<NotDerivedEmitterClass const*>();

            THEN("the cast fails")
            {
                REQUIRE(pCast == nullptr);
            }
        }
    }

    GIVEN("a pointer to a DerivedEmitterClass instance")
    {
        DerivedEmitterClass object;

        WHEN("the instance is cast into a pointer to an Object")
        {
            auto *pCast = object.tryCast<Object*>();

            THEN("the cast succeeds")
            {
                REQUIRE(pCast != nullptr);
            }
        }

        WHEN("the instance is cast into a pointer to an EmitterClass")
        {
            auto *pCast = object.tryCast<EmitterClass*>();

            THEN("the cast succeeds")
            {
                REQUIRE(pCast != nullptr);
            }
        }

        WHEN("the instance is cast into a pointer to a DerivedEmitterClass")
        {
            auto *pCast = object.tryCast<DerivedEmitterClass*>();

            THEN("the cast succeeds")
            {
                REQUIRE(pCast != nullptr);
            }
        }

        WHEN("the instance is cast into a pointer to a NotDerivedEmitterClass")
        {
            auto *pCast = object.tryCast<NotDerivedEmitterClass*>();

            THEN("the cast fails")
            {
                REQUIRE(pCast == nullptr);
            }
        }
    }

    GIVEN("a pointer to a const DerivedEmitterClass instance")
    {
        const DerivedEmitterClass object;

        WHEN("the instance is cast into a pointer to an Object")
        {
            auto *pCast = object.tryCast<Object const *>();

            THEN("the cast succeeds")
            {
                REQUIRE(pCast != nullptr);
            }
        }

        WHEN("the instance is cast into a pointer to an EmitterClass")
        {
            auto *pCast = object.tryCast<EmitterClass const*>();

            THEN("the cast succeeds")
            {
                REQUIRE(pCast != nullptr);
            }
        }

        WHEN("the instance is cast into a pointer to a DerivedEmitterClass")
        {
            auto *pCast = object.tryCast<DerivedEmitterClass const*>();

            THEN("the cast succeeds")
            {
                REQUIRE(pCast != nullptr);
            }
        }

        WHEN("the instance is cast into a pointer to a NotDerivedEmitterClass")
        {
            auto *pCast = object.tryCast<NotDerivedEmitterClass const*>();

            THEN("the cast fails")
            {
                REQUIRE(pCast == nullptr);
            }
        }
    }
}


SCENARIO("Object calls connected methods")
{
    GIVEN("a signal connected to two methods with the same signature")
    {
        EmitterClass emitter;
        TestObject obj;
        Object::connect(&emitter, &EmitterClass::signal, &obj, &TestObject::increment2);
        Object::connect(&emitter, &EmitterClass::signal, &obj, &TestObject::increment1);

        WHEN("the signal is called n times")
        {
            const auto repetitionCount = GENERATE(AS(int), 0, 1 ,3 ,12);
            for (auto i = 0; i < repetitionCount; ++i)
            {
                REQUIRE(i == obj.increment1Value());
                REQUIRE((2*i) == obj.increment2Value());
                emitter.signal();
                REQUIRE((i + 1) == obj.increment1Value());
                REQUIRE((2*(i + 1)) == obj.increment2Value());
            }

            THEN("slots are called as many times as the specified value")
            {
                REQUIRE(repetitionCount == obj.increment1Value());
                REQUIRE((2*repetitionCount) == obj.increment2Value());
            }

        }
    }

    GIVEN("a signal without arguments")
    {
        std::unique_ptr<EmitterClass> pEmitter(new EmitterClass);
        std::unique_ptr<TestObject> pReceiver(new TestObject);
        REQUIRE(0 == pReceiver->value());

        WHEN("signal is connected to a method that resets the value")
        {
            Object::connect(pEmitter.get(), &EmitterClass::signal, pReceiver.get(), &TestObject::resetValue);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == pReceiver->value());

                AND_WHEN("connected signal is emitted")
                {
                    pReceiver->setCounterInt(5);
                    REQUIRE(5 == pReceiver->value());
                    pEmitter->signal();

                    THEN("connected method gets called")
                    {
                        REQUIRE(0 == pReceiver->value());
                        pReceiver->setCounterInt(5);
                        REQUIRE(5 == pReceiver->value());

                        AND_WHEN("signal is disconnected from slot using static method")
                        {
                            Object::disconnect(pEmitter.get(), &EmitterClass::signal, pReceiver.get(), &TestObject::resetValue);

                            AND_WHEN("signal is emitted")
                            {
                                pEmitter->signal();

                                THEN("method is not called")
                                {
                                    REQUIRE(5 == pReceiver->value());

                                    AND_WHEN("connected receiver is destroyed")
                                    {
                                        pReceiver.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("signal is emitted")
                                        {
                                            pEmitter->signal();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }

                                        AND_WHEN("emitter is destroyed")
                                        {
                                            pEmitter.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }

                                    AND_WHEN("emitter is destroyed")
                                    {
                                        pEmitter.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("receiver is destroyed")
                                        {
                                            pReceiver.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("signal is disconnected from all slots using static method")
                        {
                            Object::disconnect(pEmitter.get(), &EmitterClass::signal, pReceiver.get(), nullptr);

                            AND_WHEN("signal is emitted")
                            {
                                pEmitter->signal();

                                THEN("method is not called")
                                {
                                    REQUIRE(5 == pReceiver->value());

                                    AND_WHEN("connected receiver is destroyed")
                                    {
                                        pReceiver.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("signal is emitted")
                                        {
                                            pEmitter->signal();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }

                                        AND_WHEN("emitter is destroyed")
                                        {
                                            pEmitter.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }

                                    AND_WHEN("emitter is destroyed")
                                    {
                                        pEmitter.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("receiver is destroyed")
                                        {
                                            pReceiver.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("signal is disconnected from all receivers using static method")
                        {
                            Object::disconnect(pEmitter.get(), &EmitterClass::signal, nullptr, &TestObject::resetValue);

                            AND_WHEN("signal is emitted")
                            {
                                pEmitter->signal();

                                THEN("method is not called")
                                {
                                    REQUIRE(5 == pReceiver->value());

                                    AND_WHEN("connected receiver is destroyed")
                                    {
                                        pReceiver.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("signal is emitted")
                                        {
                                            pEmitter->signal();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }

                                        AND_WHEN("emitter is destroyed")
                                        {
                                            pEmitter.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }

                                    AND_WHEN("emitter is destroyed")
                                    {
                                        pEmitter.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("receiver is destroyed")
                                        {
                                            pReceiver.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("signal is disconnected from all receivers and all slots using static method")
                        {
                            Object::disconnect(pEmitter.get(), &EmitterClass::signal, nullptr, nullptr);

                            AND_WHEN("signal is emitted")
                            {
                                pEmitter->signal();

                                THEN("method is not called")
                                {
                                    REQUIRE(5 == pReceiver->value());

                                    AND_WHEN("connected receiver is destroyed")
                                    {
                                        pReceiver.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("signal is emitted")
                                        {
                                            pEmitter->signal();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }

                                        AND_WHEN("emitter is destroyed")
                                        {
                                            pEmitter.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }

                                    AND_WHEN("emitter is destroyed")
                                    {
                                        pEmitter.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("receiver is destroyed")
                                        {
                                            pReceiver.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("all signals are disconnected from all receivers and all slots using static method")
                        {
                            Object::disconnect(pEmitter.get(), nullptr, nullptr, nullptr);

                            AND_WHEN("signal is emitted")
                            {
                                pEmitter->signal();

                                THEN("method is not called")
                                {
                                    REQUIRE(5 == pReceiver->value());

                                    AND_WHEN("connected receiver is destroyed")
                                    {
                                        pReceiver.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("signal is emitted")
                                        {
                                            pEmitter->signal();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }

                                        AND_WHEN("emitter is destroyed")
                                        {
                                            pEmitter.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }

                                    AND_WHEN("emitter is destroyed")
                                    {
                                        pEmitter.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("receiver is destroyed")
                                        {
                                            pReceiver.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("all signals are disconnected from all slots using static method")
                        {
                            Object::disconnect(pEmitter.get(), nullptr, pReceiver.get(), nullptr);

                            AND_WHEN("signal is emitted")
                            {
                                pEmitter->signal();

                                THEN("method is not called")
                                {
                                    REQUIRE(5 == pReceiver->value());

                                    AND_WHEN("connected receiver is destroyed")
                                    {
                                        pReceiver.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("signal is emitted")
                                        {
                                            pEmitter->signal();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }

                                        AND_WHEN("emitter is destroyed")
                                        {
                                            pEmitter.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }

                                    AND_WHEN("emitter is destroyed")
                                    {
                                        pEmitter.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("receiver is destroyed")
                                        {
                                            pReceiver.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("everything is disconnected from the signal using the non-static method")
                        {
                            pEmitter->disconnect(&EmitterClass::signal);

                            AND_WHEN("signal is emitted")
                            {
                                pEmitter->signal();

                                THEN("method is not called")
                                {
                                    REQUIRE(5 == pReceiver->value());

                                    AND_WHEN("connected receiver is destroyed")
                                    {
                                        pReceiver.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("signal is emitted")
                                        {
                                            pEmitter->signal();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }

                                        AND_WHEN("emitter is destroyed")
                                        {
                                            pEmitter.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }

                                    AND_WHEN("emitter is destroyed")
                                    {
                                        pEmitter.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("receiver is destroyed")
                                        {
                                            pReceiver.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("emitter is disconnected from receiver using the non-static method")
                        {
                            pEmitter->disconnect(pReceiver.get());

                            AND_WHEN("signal is emitted")
                            {
                                pEmitter->signal();

                                THEN("method is not called")
                                {
                                    REQUIRE(5 == pReceiver->value());

                                    AND_WHEN("connected receiver is destroyed")
                                    {
                                        pReceiver.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("signal is emitted")
                                        {
                                            pEmitter->signal();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }

                                        AND_WHEN("emitter is destroyed")
                                        {
                                            pEmitter.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }

                                    AND_WHEN("emitter is destroyed")
                                    {
                                        pEmitter.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("receiver is destroyed")
                                        {
                                            pReceiver.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("everything is disconnected from emitter using the non-static method")
                        {
                            pEmitter->disconnect();

                            AND_WHEN("signal is emitted")
                            {
                                pEmitter->signal();

                                THEN("method is not called")
                                {
                                    REQUIRE(5 == pReceiver->value());

                                    AND_WHEN("connected receiver is destroyed")
                                    {
                                        pReceiver.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("signal is emitted")
                                        {
                                            pEmitter->signal();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }

                                        AND_WHEN("emitter is destroyed")
                                        {
                                            pEmitter.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }

                                    AND_WHEN("emitter is destroyed")
                                    {
                                        pEmitter.reset();

                                        THEN("nothing happens")
                                        {
                                        }

                                        AND_WHEN("receiver is destroyed")
                                        {
                                            pReceiver.reset();

                                            THEN("nothing happens")
                                            {
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        AND_WHEN("connected receiver is destroyed")
                        {
                            pReceiver.reset();

                            THEN("nothing happens")
                            {
                            }

                            AND_WHEN("signal is emitted")
                            {
                                pEmitter->signal();

                                THEN("nothing happens")
                                {
                                }
                            }

                            AND_WHEN("emitter is destroyed")
                            {
                                pEmitter.reset();

                                THEN("nothing happens")
                                {
                                }
                            }
                        }

                        AND_WHEN("emitter is destroyed")
                        {
                            pEmitter.reset();

                            THEN("nothing happens")
                            {
                            }

                            AND_WHEN("receiver is destroyed")
                            {
                                pReceiver.reset();

                                THEN("nothing happens")
                                {
                                }
                            }
                        }
                    }
                }
            }
        }
    }


    GIVEN("a signal emitting an int")
    {
        EmitterClass emitter;
        TestObject object;
        REQUIRE(0 == object.value());

        WHEN("signal is connected to a method taking an int")
        {
            Object::connect(&emitter, &EmitterClass::intSignal, &object, &TestObject::setCounterInt);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a rvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    emitter.intSignal(int{genValue});

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.intSignal(lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.intSignal(constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int value = genValue;
                    int &valueRef = value;
                    emitter.intSignal(valueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constValue = genValue;
                    const int &constValueRef = constValue;
                    emitter.intSignal(constValueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const int")
        {
            Object::connect(&emitter, &EmitterClass::intSignal, &object, &TestObject::setCounterConstInt);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a rvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    emitter.intSignal(int{genValue});

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.intSignal(lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.intSignal(constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int value = genValue;
                    int &valueRef = value;
                    emitter.intSignal(valueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constValue = genValue;
                    const int &constValueRef = constValue;
                    emitter.intSignal(constValueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const int")
        {
            Object::connect(&emitter, &EmitterClass::intSignal, &object, &TestObject::setCounterConstIntRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a rvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    emitter.intSignal(int{genValue});

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.intSignal(lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.intSignal(constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int value = genValue;
                    int &valueRef = value;
                    emitter.intSignal(valueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constValue = genValue;
                    const int &constValueRef = constValue;
                    emitter.intSignal(constValueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::intSignal, &object, &TestObject::resetValue);

            THEN("signal connects to object's method")
            {
                REQUIRE(0 == object.value());
                object.setCounterInt(10);
                REQUIRE(10 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    emitter.intSignal(100);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a const method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::intSignal, &object, &TestObject::increaseOuterCounter);

            THEN("signal connects to object's method")
            {
                outerCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    emitter.intSignal(100);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }
            }
        }
    }


    GIVEN("a signal emitting a const int")
    {
        EmitterClass emitter;
        TestObject object;
        REQUIRE(0 == object.value());

        WHEN("signal is connected to a method taking an int")
        {
            Object::connect(&emitter, &EmitterClass::constIntSignal, &object, &TestObject::setCounterInt);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a rvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    emitter.constIntSignal(int{genValue});

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.constIntSignal(lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.constIntSignal(constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int value = genValue;
                    int &valueRef = value;
                    emitter.constIntSignal(valueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constValue = genValue;
                    const int &constValueRef = constValue;
                    emitter.constIntSignal(constValueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const int")
        {
            Object::connect(&emitter, &EmitterClass::constIntSignal, &object, &TestObject::setCounterConstInt);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a rvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    emitter.constIntSignal(int{genValue});

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.constIntSignal(lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.constIntSignal(constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int value = genValue;
                    int &valueRef = value;
                    emitter.constIntSignal(valueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constValue = genValue;
                    const int &constValueRef = constValue;
                    emitter.constIntSignal(constValueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const int")
        {
            Object::connect(&emitter, &EmitterClass::constIntSignal, &object, &TestObject::setCounterConstIntRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a rvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    emitter.constIntSignal(int{genValue});

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.constIntSignal(lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.constIntSignal(constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int value = genValue;
                    int &valueRef = value;
                    emitter.constIntSignal(valueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constValue = genValue;
                    const int &constValueRef = constValue;
                    emitter.constIntSignal(constValueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("the signal is connected to a method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::constIntSignal, &object, &TestObject::resetValue);

            THEN("signals connects to object's method")
            {
                REQUIRE(0 == object.value());
                object.setCounterInt(10);
                REQUIRE(10 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    emitter.constIntSignal(100);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }
            }
        }

        WHEN("the signal is connected to a const method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::constIntSignal, &object, &TestObject::increaseOuterCounter);

            THEN("signals connects to object's method")
            {
                outerCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    emitter.constIntSignal(100);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }
            }
        }
    }


    GIVEN("a signal emitting a reference to an int")
    {
        EmitterClass emitter;
        TestObject object;
        REQUIRE(0 == object.value());

        WHEN("signal is connected to a method taking an int")
        {
            Object::connect(&emitter, &EmitterClass::refIntSignal, &object, &TestObject::setCounterInt);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.refIntSignal(lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int value = genValue;
                    int &valueRef = value;
                    emitter.refIntSignal(valueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const int")
        {
            Object::connect(&emitter, &EmitterClass::refIntSignal, &object, &TestObject::setCounterConstInt);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.refIntSignal(lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int value = genValue;
                    int &valueRef = value;
                    emitter.refIntSignal(valueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a int")
        {
            Object::connect(&emitter, &EmitterClass::refIntSignal, &object, &TestObject::setCounterIntRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.refIntSignal(lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int value = genValue;
                    int &valueRef = value;
                    emitter.refIntSignal(valueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refIntSignal, &object, &TestObject::setCounterConstIntRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.refIntSignal(lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int value = genValue;
                    int &valueRef = value;
                    emitter.refIntSignal(valueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::refIntSignal, &object, &TestObject::resetValue);

            THEN("signals connects to object's method")
            {
                REQUIRE(0 == object.value());
                object.setCounterInt(10);
                REQUIRE(10 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    int value = 100;
                    emitter.refIntSignal(value);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a const method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::refIntSignal, &object, &TestObject::increaseOuterCounter);

            THEN("signals connects to object's method")
            {
                outerCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    int value = 100;
                    emitter.refIntSignal(value);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }
            }
        }
    }

    GIVEN("a signal emitting a reference to a const int")
    {
        EmitterClass emitter;
        TestObject object;
        REQUIRE(0 == object.value());

        WHEN("signal is connected to a method taking an int")
        {
            Object::connect(&emitter, &EmitterClass::refConstIntSignal, &object, &TestObject::setCounterInt);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a rvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    emitter.refConstIntSignal(int{genValue});

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.refConstIntSignal(lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.refConstIntSignal(constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int value = genValue;
                    int &valueRef = value;
                    emitter.refConstIntSignal(valueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constValue = genValue;
                    const int &constValueRef = constValue;
                    emitter.refConstIntSignal(constValueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const int")
        {
            Object::connect(&emitter, &EmitterClass::refConstIntSignal, &object, &TestObject::setCounterConstInt);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a rvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    emitter.refConstIntSignal(int{genValue});

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.refConstIntSignal(lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.refConstIntSignal(constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int value = genValue;
                    int &valueRef = value;
                    emitter.refConstIntSignal(valueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constValue = genValue;
                    const int &constValueRef = constValue;
                    emitter.refConstIntSignal(constValueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refConstIntSignal, &object, &TestObject::setCounterConstIntRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a rvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    emitter.refConstIntSignal(int{genValue});

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.refConstIntSignal(lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.refConstIntSignal(constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int value = genValue;
                    int &valueRef = value;
                    emitter.refConstIntSignal(valueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const reference to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constValue = genValue;
                    const int &constValueRef = constValue;
                    emitter.refConstIntSignal(constValueRef);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::refConstIntSignal, &object, &TestObject::resetValue);

            THEN("signals connects to object's method")
            {
                REQUIRE(0 == object.value());
                object.setCounterInt(10);
                REQUIRE(10 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    emitter.refConstIntSignal(100);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a const method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::refConstIntSignal, &object, &TestObject::increaseOuterCounter);

            THEN("signals connects to object's method")
            {
                outerCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    emitter.refConstIntSignal(100);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }
            }
        }
    }

    GIVEN("a signal emitting a pointer to an int")
    {
        EmitterClass emitter;
        TestObject object;
        REQUIRE(0 == object.value());

        WHEN("signal is connected to a method taking a pointer to an int")
        {
            Object::connect(&emitter, &EmitterClass::ptrIntSignal, &object, &TestObject::setCounterIntPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.ptrIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const pointer to an int")
        {
            Object::connect(&emitter, &EmitterClass::ptrIntSignal, &object, &TestObject::setCounterIntConstPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.ptrIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::ptrIntSignal, &object, &TestObject::setCounterConstIntPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.ptrIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::ptrIntSignal, &object, &TestObject::setCounterConstIntConstPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.ptrIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const pointer to a int")
        {
            Object::connect(&emitter, &EmitterClass::ptrIntSignal, &object, &TestObject::setCounterIntConstPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.ptrIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::ptrIntSignal, &object, &TestObject::setCounterConstIntConstPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.ptrIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::ptrIntSignal, &object, &TestObject::resetValue);

            THEN("signal connects to object's method")
            {
                REQUIRE(0 == object.value());
                object.setCounterInt(10);
                REQUIRE(10 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    int value = 100;
                    emitter.ptrIntSignal(&value);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a const method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::ptrIntSignal, &object, &TestObject::increaseOuterCounter);

            THEN("signal connects to object's method")
            {
                outerCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    int value = 100;
                    emitter.ptrIntSignal(&value);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }
            }
        }
    }

    GIVEN("a signal emitting a pointer to a const int")
    {
        EmitterClass emitter;
        TestObject object;
        REQUIRE(0 == object.value());

        WHEN("signal is connected to a method taking a pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::ptrConstIntSignal, &object, &TestObject::setCounterConstIntPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.ptrConstIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a pointer to a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.ptrConstIntSignal(&constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::ptrConstIntSignal, &object, &TestObject::setCounterConstIntConstPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.ptrConstIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a pointer to a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.ptrConstIntSignal(&constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::ptrConstIntSignal, &object, &TestObject::setCounterConstIntConstPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.ptrConstIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a pointer to a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.ptrConstIntSignal(&constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::ptrConstIntSignal, &object, &TestObject::resetValue);

            THEN("signal connects to object's method")
            {
                REQUIRE(0 == object.value());
                object.setCounterInt(10);
                REQUIRE(10 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    const int value = 100;
                    emitter.ptrConstIntSignal(&value);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a const method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::ptrConstIntSignal, &object, &TestObject::increaseOuterCounter);

            THEN("signal connects to object's method")
            {
                outerCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    const int value = 100;
                    emitter.ptrConstIntSignal(&value);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }
            }
        }
    }

    GIVEN("a signal emitting a const pointer to an int")
    {
        EmitterClass emitter;
        TestObject object;
        REQUIRE(0 == object.value());

        WHEN("signal is connected to a method taking a pointer to a int")
        {
            Object::connect(&emitter, &EmitterClass::constPtrIntSignal, &object, &TestObject::setCounterIntPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.constPtrIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const pointer to a int")
        {
            Object::connect(&emitter, &EmitterClass::constPtrIntSignal, &object, &TestObject::setCounterIntConstPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.constPtrIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::constPtrIntSignal, &object, &TestObject::setCounterConstIntPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.constPtrIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::constPtrIntSignal, &object, &TestObject::setCounterConstIntConstPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.constPtrIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const pointer to int")
        {
            Object::connect(&emitter, &EmitterClass::constPtrIntSignal, &object, &TestObject::setCounterIntConstPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.constPtrIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::constPtrIntSignal, &object, &TestObject::setCounterConstIntConstPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.constPtrIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::constPtrIntSignal, &object, &TestObject::resetValue);

            THEN("signal connects to object's method")
            {
                REQUIRE(0 == object.value());
                object.setCounterInt(10);
                REQUIRE(10 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    int value = 100;
                    emitter.constPtrIntSignal(&value);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a const method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::constPtrIntSignal, &object, &TestObject::increaseOuterCounter);

            THEN("signal connects to object's method")
            {
                outerCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    int value = 100;
                    emitter.constPtrIntSignal(&value);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }
            }
        }
    }

    GIVEN("a signal emitting a const pointer to a const int")
    {
        EmitterClass emitter;
        TestObject object;
        REQUIRE(0 == object.value());

        WHEN("signal is connected to a method taking a pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::constPtrConstIntSignal, &object, &TestObject::setCounterConstIntPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.constPtrConstIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a pointer to a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.constPtrConstIntSignal(&constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::constPtrConstIntSignal, &object, &TestObject::setCounterConstIntConstPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.constPtrConstIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a pointer to a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.constPtrConstIntSignal(&constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::constPtrConstIntSignal, &object, &TestObject::setCounterConstIntConstPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    emitter.constPtrConstIntSignal(&lValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a pointer to a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int constLValue = genValue;
                    emitter.constPtrConstIntSignal(&constLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::constPtrConstIntSignal, &object, &TestObject::resetValue);

            THEN("signal connects to object's method")
            {
                REQUIRE(0 == object.value());
                object.setCounterInt(10);
                REQUIRE(10 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    const int value = 100;
                    emitter.constPtrConstIntSignal(&value);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a const method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::constPtrConstIntSignal, &object, &TestObject::increaseOuterCounter);

            THEN("signal connects to object's method")
            {
                outerCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    const int value = 100;
                    emitter.constPtrConstIntSignal(&value);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }
            }
        }
    }

    GIVEN("a signal emitting a reference to pointer to an int")
    {
        EmitterClass emitter;
        TestObject object;
        REQUIRE(0 == object.value());

        WHEN("signal is connected to a method taking a pointer to an int")
        {
            Object::connect(&emitter, &EmitterClass::refPtrIntSignal, &object, &TestObject::setCounterIntPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    int *pLValue = &lValue;
                    emitter.refPtrIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refPtrIntSignal, &object, &TestObject::setCounterConstIntPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    int *pLValue = &lValue;
                    emitter.refPtrIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const pointer to a int")
        {
            Object::connect(&emitter, &EmitterClass::refPtrIntSignal, &object, &TestObject::setCounterIntConstPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    int *pLValue = &lValue;
                    emitter.refPtrIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refPtrIntSignal, &object, &TestObject::setCounterConstIntConstPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    int *pLValue = &lValue;
                    emitter.refPtrIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a pointer to an int")
        {
            Object::connect(&emitter, &EmitterClass::refPtrIntSignal, &object, &TestObject::setCounterIntPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    int *pLValue = &lValue;
                    emitter.refPtrIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const pointer to an int")
        {
            Object::connect(&emitter, &EmitterClass::refPtrIntSignal, &object, &TestObject::setCounterIntConstPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    int *pLValue = &lValue;
                    emitter.refPtrIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refPtrIntSignal, &object, &TestObject::setCounterConstIntConstPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    int *pLValue = &lValue;
                    emitter.refPtrIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::refPtrIntSignal, &object, &TestObject::resetValue);

            THEN("signals connects to object's method")
            {
                REQUIRE(0 == object.value());
                object.setCounterInt(10);
                REQUIRE(10 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    int value = 100;
                    int *pValue = &value;
                    emitter.refPtrIntSignal(pValue);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a const method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::refPtrIntSignal, &object, &TestObject::increaseOuterCounter);

            THEN("signals connects to object's method")
            {
                outerCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    int value = 100;
                    int *pValue = &value;
                    emitter.refPtrIntSignal(pValue);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }
            }
        }
    }

    GIVEN("a signal emitting a reference to a const pointer to an int")
    {
        EmitterClass emitter;
        TestObject object;
        REQUIRE(0 == object.value());

        WHEN("signal is connected to a method taking a pointer to an int")
        {
            Object::connect(&emitter, &EmitterClass::refConstPtrIntSignal, &object, &TestObject::setCounterIntPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    int *pLValue = &lValue;
                    emitter.refConstPtrIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refConstPtrIntSignal, &object, &TestObject::setCounterConstIntPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    int *pLValue = &lValue;
                    emitter.refConstPtrIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const pointer to an int")
        {
            Object::connect(&emitter, &EmitterClass::refConstPtrIntSignal, &object, &TestObject::setCounterIntConstPtr);

            THEN("signal connects to the object's method")
            {               
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    int *pLValue = &lValue;
                    emitter.refConstPtrIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refConstPtrIntSignal, &object, &TestObject::setCounterConstIntConstPtr);

            THEN("signal connects to the object's method")
            {               
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    int *pLValue = &lValue;
                    emitter.refConstPtrIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const pointer to a int")
        {
            Object::connect(&emitter, &EmitterClass::refConstPtrIntSignal, &object, &TestObject::setCounterIntConstPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    int *pLValue = &lValue;
                    emitter.refConstPtrIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refConstPtrIntSignal, &object, &TestObject::setCounterConstIntConstPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    int lValue = genValue;
                    int *pLValue = &lValue;
                    emitter.refConstPtrIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::refConstPtrIntSignal, &object, &TestObject::resetValue);

            THEN("signals connects to object's method")
            {
                REQUIRE(0 == object.value());
                object.setCounterInt(10);
                REQUIRE(10 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    int value = 100;
                    int *pValue = &value;
                    emitter.refConstPtrIntSignal(pValue);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a const method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::refConstPtrIntSignal, &object, &TestObject::increaseOuterCounter);

            THEN("signals connects to object's method")
            {
                outerCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    int value = 100;
                    int *pValue = &value;
                    emitter.refConstPtrIntSignal(pValue);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }
            }
        }
    }

    GIVEN("a signal emitting a reference to a pointer to a const int")
    {
        EmitterClass emitter;
        TestObject object;
        REQUIRE(0 == object.value());

        WHEN("signal is connected to a method taking a pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refPtrConstIntSignal, &object, &TestObject::setCounterConstIntPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int lValue = genValue;
                    const int *pLValue = &lValue;
                    emitter.refPtrConstIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refPtrConstIntSignal, &object, &TestObject::setCounterConstIntConstPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int lValue = genValue;
                    const int *pLValue = &lValue;
                    emitter.refPtrConstIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refPtrConstIntSignal, &object, &TestObject::setCounterConstIntPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int lValue = genValue;
                    const int *pLValue = &lValue;
                    emitter.refPtrConstIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refPtrConstIntSignal, &object, &TestObject::setCounterConstIntConstPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int lValue = genValue;
                    const int *pLValue = &lValue;
                    emitter.refPtrConstIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::refPtrConstIntSignal, &object, &TestObject::resetValue);

            THEN("signals connects to object's method")
            {
                REQUIRE(0 == object.value());
                object.setCounterInt(10);
                REQUIRE(10 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    const int value = 100;
                    const int *pValue = &value;
                    emitter.refPtrConstIntSignal(pValue);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a const method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::refPtrConstIntSignal, &object, &TestObject::increaseOuterCounter);

            THEN("signals connects to object's method")
            {
                outerCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    const int value = 100;
                    const int *pValue = &value;
                    emitter.refPtrConstIntSignal(pValue);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }
            }
        }
    }

    GIVEN("a signal emitting a reference to a const pointer to a const int")
    {
        EmitterClass emitter;
        TestObject object;
        REQUIRE(0 == object.value());

        WHEN("signal is connected to a method taking a pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refConstPtrConstIntSignal, &object, &TestObject::setCounterConstIntPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int lValue = genValue;
                    const int *pLValue = &lValue;
                    emitter.refConstPtrConstIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refConstPtrConstIntSignal, &object, &TestObject::setCounterConstIntConstPtr);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int lValue = genValue;
                    const int *pLValue = &lValue;
                    emitter.refConstPtrConstIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("signal is connected to a method taking a reference to a const pointer to a const int")
        {
            Object::connect(&emitter, &EmitterClass::refConstPtrConstIntSignal, &object, &TestObject::setCounterConstIntConstPtrRef);

            THEN("signal connects to the object's method")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted with a pointer to a const lvalue")
                {
                    const int genValue = GENERATE(AS(int), INT_MIN, INT_MIN +1234, -5, 0, 5, 123456, INT_MAX - 1234, INT_MAX);
                    const int lValue = genValue;
                    const int *pLValue = &lValue;
                    emitter.refConstPtrConstIntSignal(pLValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(genValue == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::refConstPtrConstIntSignal, &object, &TestObject::resetValue);

            THEN("signals connects to object's method")
            {
                REQUIRE(0 == object.value());
                object.setCounterInt(10);
                REQUIRE(10 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    const int value = 100;
                    const int *pValue = &value;
                    emitter.refConstPtrConstIntSignal(pValue);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a const method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::refConstPtrConstIntSignal, &object, &TestObject::increaseOuterCounter);

            THEN("signals connects to object's method")
            {
                outerCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    const int value = 100;
                    const int *pValue = &value;
                    emitter.refConstPtrConstIntSignal(pValue);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }
            }
        }
    }

    GIVEN("a signal emitting two ints")
    {
        EmitterClass emitter;
        TestObject object;
        REQUIRE(0 == object.value());

        WHEN("the signal is connected to a method that takes two integers")
        {
            Object::connect(&emitter, &EmitterClass::twoIntsSignal, &object, &TestObject::setValues);

            THEN("signal connects to the object")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    const auto emittedValue = GENERATE(AS(int), -125, 0, 1, 3, 5, 12);
                    WARN(QByteArray("emitted value: ").append(QByteArray::number(emittedValue)));
                    const int delta = 18;
                    emitter.twoIntsSignal(emittedValue, emittedValue + delta);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(emittedValue == object.value1());
                        REQUIRE((emittedValue + delta) == object.value2());
                    }
                }
            }
        }

        WHEN("the signal is connected to a method that takes less arguments")
        {
            Object::connect(&emitter, &EmitterClass::twoIntsSignal, &object, &TestObject::setCounterInt);

            THEN("signal connects to the object")
            {
                REQUIRE(0 == object.value());

                AND_WHEN("connected signal is emitted")
                {
                    const auto emittedValue = GENERATE(AS(int), -125, 0, 1, 3, 5, 12);
                    WARN(QByteArray("emitted value: ").append(QByteArray::number(emittedValue)));
                    emitter.twoIntsSignal(emittedValue, emittedValue);

                    THEN("method on connected object is called")
                    {
                        REQUIRE(emittedValue == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::twoIntsSignal, &object, &TestObject::resetValue);

            THEN("signals connects to object's method")
            {
                REQUIRE(0 == object.value());
                object.setCounterInt(10);
                REQUIRE(10 == object.value());

                AND_WHEN("connected signal is emitted with a rvalue")
                {
                    emitter.twoIntsSignal(100, 100);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a int")
                {
                    int value = 100;
                    emitter.twoIntsSignal(value, value);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }

                AND_WHEN("connected signal is emitted with a const int")
                {
                    const int value = 100;
                    emitter.twoIntsSignal(value, value);

                    THEN("connected method is called")
                    {
                        REQUIRE(0 == object.value());
                    }
                }
            }
        }

        WHEN("a signal is connected to a const method with no arguments")
        {
            Object::connect(&emitter, &EmitterClass::twoIntsSignal, &object, &TestObject::increaseOuterCounter);

            THEN("signals connects to object's method")
            {
                outerCounter = 0;

                AND_WHEN("connected signal is emitted with a rvalue")
                {
                    emitter.twoIntsSignal(100, 100);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }

                AND_WHEN("connected signal is emitted with a int")
                {
                    int value = 100;
                    emitter.twoIntsSignal(value, value);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }

                AND_WHEN("connected signal is emitted with a const int")
                {
                    const int value = 100;
                    emitter.twoIntsSignal(value, value);

                    THEN("connected method is called")
                    {
                        REQUIRE(1 == outerCounter);
                    }
                }
            }
        }
    }
}


namespace Test::Object
{

class TestManager : public Kourier::Object
{
KOURIER_OBJECT(Test::Object::TestManager)
public:
    enum class ActionType {DoNothing, ConnectAnother, DisconnectYourself};
    TestManager(EmitterClass &emitter, int recursionLevel = 0) :
        m_pEmitter(&emitter),
        m_recursionLevel(recursionLevel)
    {
        m_object1.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object1.get(), &TestObject::increaseOuterCounter);
        m_object2.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object2.get(), &TestObject::increaseOuterCounter);
        m_object3.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object3.get(), &TestObject::increaseOuterCounter);
        m_object4.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object4.get(), &TestObject::increaseOuterCounter);
        Object::connect(m_pEmitter, &EmitterClass::signal, this, &TestManager::doAction);
        m_object5.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object5.get(), &TestObject::increaseOuterCounter);
        m_object6.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object6.get(), &TestObject::increaseOuterCounter);
        m_object7.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object7.get(), &TestObject::increaseOuterCounter);
        m_object8.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object8.get(), &TestObject::increaseOuterCounter);
        m_object9.reset(new TestObject);
    }
    ~TestManager() override = default;

    void doAction()
    {
        if (m_firstRun)
        {
            m_firstRun = false;
            m_pEmitter->disconnect(m_object2.get());
            m_object3.reset();
            switch (m_actionType)
            {
                case ActionType::DoNothing:
                    ++outerCounter;
                    break;
                case ActionType::ConnectAnother:
                    outerCounter += 5;
                    Object::connect(m_pEmitter, &EmitterClass::signal, m_object9.get(), &TestObject::increaseOuterCounter);
                    break;
                case ActionType::DisconnectYourself:
                    outerCounter += 10;
                    m_pEmitter->disconnect(this);
                    break;
            }
            m_pEmitter->disconnect(m_object6.get());
            m_object7.reset();
            if (m_recursionLevel-- > 0)
                m_pEmitter->signal();
        }
        else
        {
            ++outerCounter;
            if (m_recursionLevel-- > 0)
                m_pEmitter->signal();
        }
    }

    void setActionType(ActionType actionType) {m_actionType = actionType;}

private:
    bool m_firstRun = true;
    int m_recursionLevel = 0;
    ActionType m_actionType = ActionType::DoNothing;
    EmitterClass *m_pEmitter = nullptr;
    std::unique_ptr<TestObject> m_object1;
    std::unique_ptr<TestObject> m_object2;
    std::unique_ptr<TestObject> m_object3;
    std::unique_ptr<TestObject> m_object4;
    std::unique_ptr<TestObject> m_object5;
    std::unique_ptr<TestObject> m_object6;
    std::unique_ptr<TestObject> m_object7;
    std::unique_ptr<TestObject> m_object8;
    std::unique_ptr<TestObject> m_object9;
};

}

SCENARIO("Object supports connection/disconnection/destruction while emitting")
{
    GIVEN("A signal that emits no argument and a test manager")
    {
        EmitterClass emitter;
        TestManager testManager(emitter);
        outerCounter = 0;

        WHEN("signal is emitted for a test manager that is instructed to do nothing when its method gets called")
        {
            testManager.setActionType(TestManager::ActionType::DoNothing);
            emitter.signal();

            THEN("outerCounter is increased 7 times")
            {
                REQUIRE(7 == outerCounter);
            }
        }

        WHEN("signal is emitted for a test manager that is instructed to disconnect from signal when its method gets called")
        {
            testManager.setActionType(TestManager::ActionType::DisconnectYourself);
            emitter.signal();

            THEN("outerCounter is increased 16 times")
            {
                REQUIRE(16 == outerCounter);
            }
        }

        WHEN("signal is emitted for a test manager that is instructed to connect another object when its method gets called")
        {
            testManager.setActionType(TestManager::ActionType::ConnectAnother);
            emitter.signal();

            THEN("outerCounter is increased 11 times")
            {
                REQUIRE(11 == outerCounter);
            }
        }
    }
}


namespace Test::Object
{

class RecursiveEmitter : public Kourier::Object
{
KOURIER_OBJECT(Test::Object::RecursiveEmitter)
public:
    RecursiveEmitter(EmitterClass &emitter, int recursionLevel) :
  m_pEmitter(&emitter),
        m_recursionLevel(recursionLevel)
    {
        assert(m_recursionLevel > 0);
        Object::connect(m_pEmitter, &EmitterClass::signal, this, &RecursiveEmitter::slot);
    }

    ~RecursiveEmitter() override = default;

    void slot()
    {
        ++m_value;
        if (m_recursionLevel-- > 0)
            m_pEmitter->signal();
    }

    int value() const {return m_value;}

private:
    EmitterClass *m_pEmitter;
    int m_value = 0;
    int m_recursionLevel;
};

}


SCENARIO("Object supports recursive emission")
{
    GIVEN("a recursive emitter object")
    {
        EmitterClass emitter;
        const int recursionLevel = GENERATE(AS(int), 1, 5, 10);
        RecursiveEmitter recursiveEmitter(emitter, recursionLevel);

        WHEN("signal is emitted")
        {
            emitter.signal();

            THEN("signal is emitted recursivelly as many times as the specified recursion level")
            {
                REQUIRE((recursionLevel + 1) == recursiveEmitter.value());
            }
        }
    }
}


SCENARIO("Object supports connection/disconnection/destruction while emitting recursivelly")
{
    GIVEN("A signal that emits no argument and a test manager")
    {
        EmitterClass emitter;
        const int recursionLevel = GENERATE(AS(int), 1, 5, 10);
        TestManager testManager(emitter, recursionLevel);
        outerCounter = 0;

        WHEN("signal is emitted for a test manager that is instructed to do nothing when its method gets called")
        {
            testManager.setActionType(TestManager::ActionType::DoNothing);
            emitter.signal();

            THEN("outerCounter is increased (2 + 5 * (recursionLevel + 1)) times")
            {
                REQUIRE((2 + 5 * (recursionLevel + 1)) == outerCounter);
            }
        }

        WHEN("signal is emitted for a test manager that is instructed to disconnect from signal when its method gets called")
        {
            testManager.setActionType(TestManager::ActionType::DisconnectYourself);
            emitter.signal();

            THEN("outerCounter is increased 20 times")
            {
                REQUIRE(20 == outerCounter);
            }
        }

        WHEN("signal is emitted for a test manager that is instructed to connect another object when its method gets called")
        {
            testManager.setActionType(TestManager::ActionType::ConnectAnother);
            emitter.signal();

            THEN("outerCounter is increased (10 + recursionLevel * 6 + 1) times")
            {
                REQUIRE((10 + recursionLevel * 6 + 1) == outerCounter);
            }
        }
    }
}


SCENARIO("Object supports functors")
{
    GIVEN("a functor connected to a signal")
    {
        int value = 0;
        EmitterClass emitter;
        std::unique_ptr<Object> ctxObject(new Object);
        Object::connect(&emitter, &EmitterClass::signal, ctxObject.get(), [&value](){++value;});

        WHEN("signal is emitted")
        {
            emitter.signal();

            THEN("value is incremented")
            {
                REQUIRE(1 == value);

                AND_WHEN("signal is emitted again")
                {
                    emitter.signal();

                    THEN("value is incremented")
                    {
                        REQUIRE(2 == value);
                    }
                }

                AND_WHEN("context object is destroyed")
                {
                    ctxObject.reset();

                    THEN("signal disconnects from functor")
                    {
                        AND_WHEN("signal is emitted")
                        {
                            emitter.signal();

                            THEN("value is not incremented")
                            {
                                REQUIRE(1 == value);
                            }
                        }
                    }
                }
            }
        }

        WHEN("context object is destroyed")
        {
            ctxObject.reset();

            THEN("signal disconnects from functor")
            {
                AND_WHEN("signal is emitted")
                {
                    emitter.signal();

                    THEN("value is not incremented")
                    {
                        REQUIRE(0 == value);
                    }
                }
            }
        }
    }
}


namespace Test::Object
{

class FunctorTestManager : public Kourier::Object
{
KOURIER_OBJECT(Test::Object::FunctorTestManager)
public:
    enum class ActionType {DoNothing, ConnectAnother, DisconnectYourself};
    FunctorTestManager(EmitterClass &emitter, int recursionLevel = 0) :
        m_pEmitter(&emitter),
        m_recursionLevel(recursionLevel)
    {
        m_object1.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object1.get(), [](){++outerCounter;});
        m_object2.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object2.get(), [](){++outerCounter;});
        m_object3.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object3.get(), [](){++outerCounter;});
        m_object4.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object4.get(), [](){++outerCounter;});
        Object::connect(m_pEmitter, &EmitterClass::signal, this, &FunctorTestManager::doAction);
        m_object5.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object5.get(), [](){++outerCounter;});
        m_object6.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object6.get(), [](){++outerCounter;});
        m_object7.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object7.get(), [](){++outerCounter;});
        m_object8.reset(new TestObject);
        Object::connect(m_pEmitter, &EmitterClass::signal, m_object8.get(), [](){++outerCounter;});
        m_object9.reset(new TestObject);
    }

    ~FunctorTestManager() override = default;

    void doAction()
    {
        if (m_firstRun)
        {
            m_firstRun = false;
            m_pEmitter->disconnect(m_object2.get());
            m_object3.reset();
            switch (m_actionType)
            {
                case ActionType::DoNothing:
                    ++outerCounter;
                    break;
                case ActionType::ConnectAnother:
                    outerCounter += 5;
                    Object::connect(m_pEmitter, &EmitterClass::signal, m_object9.get(), [](){++outerCounter;});
                    break;
                case ActionType::DisconnectYourself:
                    outerCounter += 10;
                    m_pEmitter->disconnect(this);
                    break;
            }
            m_pEmitter->disconnect(m_object6.get());
            m_object7.reset();
            if (m_recursionLevel-- > 0)
                m_pEmitter->signal();
        }
        else
        {
            ++outerCounter;
            if (m_recursionLevel-- > 0)
                m_pEmitter->signal();
        }
    }

    void setActionType(ActionType actionType) {m_actionType = actionType;}

private:
    bool m_firstRun = true;
    int m_recursionLevel = 0;
    ActionType m_actionType = ActionType::DoNothing;
    EmitterClass *m_pEmitter = nullptr;
    std::unique_ptr<TestObject> m_object1;
    std::unique_ptr<TestObject> m_object2;
    std::unique_ptr<TestObject> m_object3;
    std::unique_ptr<TestObject> m_object4;
    std::unique_ptr<TestObject> m_object5;
    std::unique_ptr<TestObject> m_object6;
    std::unique_ptr<TestObject> m_object7;
    std::unique_ptr<TestObject> m_object8;
    std::unique_ptr<TestObject> m_object9;
};

}


SCENARIO("Object supports functor connection/disconnection/destruction while emitting")
{
    GIVEN("A signal that emits no argument and a functor test manager")
    {
        EmitterClass emitter;
        FunctorTestManager testManager(emitter);
        outerCounter = 0;

        WHEN("signal is emitted for a test manager that is instructed to do nothing when its method gets called")
        {
            testManager.setActionType(FunctorTestManager::ActionType::DoNothing);
            emitter.signal();

            THEN("outerCounter is increased 7 times")
            {
                REQUIRE(7 == outerCounter);
            }
        }

        WHEN("signal is emitted for a test manager that is instructed to disconnect from signal when its method gets called")
        {
            testManager.setActionType(FunctorTestManager::ActionType::DisconnectYourself);
            emitter.signal();

            THEN("outerCounter is increased 16 times")
            {
                REQUIRE(16 == outerCounter);
            }
        }

        WHEN("signal is emitted for a test manager that is instructed to connect another object when its method gets called")
        {
            testManager.setActionType(FunctorTestManager::ActionType::ConnectAnother);
            emitter.signal();

            THEN("outerCounter is increased 11 times")
            {
                REQUIRE(11 == outerCounter);
            }
        }
    }
}


class FunctorRecursiveEmitter : public Object
{
KOURIER_OBJECT(FunctorRecursiveEmitter)
public:
    FunctorRecursiveEmitter(EmitterClass &emitter, int recursionLevel) :
        m_pEmitter(&emitter),
        m_recursionLevel(recursionLevel)
    {
        assert(m_recursionLevel > 0);
        Object::connect(&emitter, &EmitterClass::signal, this, [this]()
        {
            ++m_value;
            if (m_recursionLevel-- > 0)
                m_pEmitter->signal();
        });
    }
    ~FunctorRecursiveEmitter() override = default;

    int value() const {return m_value;}

private:
    EmitterClass *m_pEmitter;
    int m_value = 0;
    int m_recursionLevel;
};



SCENARIO("Object supports recursive emission with functors")
{
    GIVEN("a recursive emitter object")
    {
        EmitterClass emitter;
        const int recursionLevel = GENERATE(AS(int), 1, 5, 10);
        FunctorRecursiveEmitter recursiveEmitter(emitter, recursionLevel);

        WHEN("signal is emitted")
        {
            emitter.signal();

            THEN("signal is emitted recursivelly as many times as the specified recursion level")
            {
                REQUIRE((recursionLevel + 1) == recursiveEmitter.value());
            }
        }
    }
}


SCENARIO("Object supports functor connection/disconnection/destruction while emitting recursivelly")
{
    GIVEN("A signal that emits no argument and a test manager")
    {
        EmitterClass emitter;
        const int recursionLevel = GENERATE(AS(int), 1, 5, 10);
        FunctorTestManager testManager(emitter, recursionLevel);
        outerCounter = 0;

        WHEN("signal is emitted for a test manager that is instructed to do nothing when its method gets called")
        {
            testManager.setActionType(FunctorTestManager::ActionType::DoNothing);
            emitter.signal();

            THEN("outerCounter is increased (2 + 5 * (recursionLevel + 1) times")
            {
                REQUIRE((2 + 5 * (recursionLevel + 1)) == outerCounter);
            }
        }

        WHEN("signal is emitted for a test manager that is instructed to disconnect from signal when its method gets called")
        {
            testManager.setActionType(FunctorTestManager::ActionType::DisconnectYourself);
            emitter.signal();

            THEN("outerCounter is increased 20 times")
            {
                REQUIRE(20 == outerCounter);
            }
        }

        WHEN("signal is emitted for a test manager that is instructed to connect another object when its method gets called")
        {
            testManager.setActionType(FunctorTestManager::ActionType::ConnectAnother);
            emitter.signal();

            THEN("outerCounter is increased (10 + recursionLevel * 6 + 1) times")
            {
                REQUIRE((10 + recursionLevel * 6 + 1) == outerCounter);
            }
        }
    }
}


SCENARIO("Object supports connecting to multiple objects")
{
    GIVEN("a signal connected to multiple objects")
    {
        outerCounter = 0;
        EmitterClass emitter;
        TestObject testObject[10] = {};
        for (auto i = 0; i < 10; ++i)
            Object::connect(&emitter, &EmitterClass::signal, &testObject[i], &TestObject::increaseOuterCounter);

        WHEN("signal is emitted")
        {
            emitter.signal();

            THEN("all connected objects are called")
            {
                REQUIRE(10 == outerCounter);
            }
        }
    }

    GIVEN("a signal emitting an int connected to multiple objects")
    {
        EmitterClass emitter;
        TestObject testObject[10] = {};
        for (auto i = 0; i < 10; ++i)
            Object::connect(&emitter, &EmitterClass::intSignal, &testObject[i], &TestObject::setCounterInt);

        WHEN("signal is emitted")
        {
            for (auto i = 0; i < 10; ++i)
                REQUIRE(0 == testObject[i].value());
            const auto value = GENERATE(AS(int), -12, 0, 255);
            emitter.intSignal(value);

            THEN("all connected objects are called")
            {
                for (auto i = 0; i < 10; ++i)
                    REQUIRE(value == testObject[i].value());
            }
        }
    }
}


SCENARIO("Object supports connecting to multiple signals")
{
    GIVEN("an object connected to multiple signals")
    {
        outerCounter = 0;
        EmitterClass emitters[10] = {};
        TestObject testObject;
        for (auto i = 0; i < 10; ++i)
            Object::connect(&emitters[i], &EmitterClass::signal, &testObject, &TestObject::increaseOuterCounter);

        WHEN("signals are emitted")
        {
            for (auto i = 0; i < 10; ++i)
                emitters[i].signal();

            THEN("all signals call connected object")
            {
                REQUIRE(10 == outerCounter);
            }
        }
    }

    GIVEN("a signal emitting an int connected to multiple objects")
    {
        EmitterClass emitters[10] = {};
        TestObject testObject;
        for (auto i = 0; i < 10; ++i)
            Object::connect(&emitters[i], &EmitterClass::intSignal, &testObject, &TestObject::increaseValue);

        WHEN("signal is emitted")
        {
            const auto value = GENERATE(AS(int), -12, 0, 255);
            REQUIRE(0 == testObject.value());
            for (auto i = 0; i < 10; ++i)
                emitters[i].intSignal(value);

            THEN("all signals call connected object")
            {
                for (auto i = 0; i < 10; ++i)
                    REQUIRE((10 * value) == testObject.value());
            }
        }
    }
}


namespace Test::Object
{

class MultiArgsTypesEmitter : public Kourier::Object
{
KOURIER_OBJECT(Test::Object::MultiArgsTypesEmitter)
public:
    MultiArgsTypesEmitter() = default;
    ~MultiArgsTypesEmitter() override = default;
    Kourier::Signal signal(QString string, QByteArray byteArray, QList<int> list);
};

Kourier::Signal MultiArgsTypesEmitter::signal(QString string, QByteArray byteArray, QList<int> list) KOURIER_SIGNAL(&MultiArgsTypesEmitter::signal, string, byteArray, list)

}


SCENARIO("Object supports signals with different argument types")
{
    GIVEN("a signal with different argument types connected to a functor")
    {
        MultiArgsTypesEmitter emitter;
        QString receivedQString;
        QByteArray receivedQByteArray;
        QList<int> receivedList;
        Object::connect(&emitter, &MultiArgsTypesEmitter::signal, [&](QString string, QByteArray byteArray, QList<int> list)
        {
            receivedQString = string;
            receivedQByteArray = byteArray;
            receivedList = list;
        });

        WHEN("signal is emitted")
        {
            const QString emittedString("This is the emitted string");
            const QByteArray emittedByteArray("This is the emitted byte array");
            const QList<int> emittedList = QList<int>() << 1 << 25 << 35;
            emitter.signal(emittedString, emittedByteArray, emittedList);

            THEN("functor received sent arguments")
            {
                REQUIRE(emittedString == receivedQString);
                REQUIRE(emittedByteArray == receivedQByteArray);
                REQUIRE(emittedList == receivedList);
            }
        }
    }
}


static int functionCounter = 0;
static void increaseFunctionCounter()
{
    ++functionCounter;
}


SCENARIO("Object connects to functions")
{
    GIVEN("an object")
    {
        EmitterClass emitter;

        WHEN("object is connected to a function")
        {
            Object::connect(&emitter, &EmitterClass::signal, increaseFunctionCounter);

            THEN("object connects to function")
            {
                functionCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    emitter.signal();

                    THEN("connected function is called")
                    {
                        REQUIRE(1 == functionCounter);
                    }
                }
            }
        }

        WHEN("object is connected to a function and a context object")
        {
            std::unique_ptr<Object> pCtxObject(new Object);
            Object::connect(&emitter, &EmitterClass::signal, pCtxObject.get(), increaseFunctionCounter);

            THEN("object connects to function")
            {
                functionCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    emitter.signal();

                    THEN("connected function is called")
                    {
                        REQUIRE(1 == functionCounter);

                        AND_WHEN("signal is emitted again")
                        {
                            emitter.signal();

                            THEN("connected function is called one more time")
                            {
                                REQUIRE(2 == functionCounter);
                            }
                        }

                        AND_WHEN("context object is destryed")
                        {
                            pCtxObject.reset();

                            AND_WHEN("connected signal is emitted")
                            {
                                emitter.signal();

                                THEN("connected function is not called again")
                                {
                                    REQUIRE(1 == functionCounter);
                                }
                            }
                        }
                    }
                }

                AND_WHEN("context object is destryed")
                {
                    pCtxObject.reset();

                    functionCounter = 0;

                    AND_WHEN("connected signal is emitted")
                    {
                        emitter.signal();

                        THEN("connected function is not called")
                        {
                            REQUIRE(0 == functionCounter);
                        }
                    }
                }
            }
        }
    }
}

extern "C"
{
static void cIncreaseFunctionCounter()
{
    ++functionCounter;
}
}

SCENARIO("Object connects to functions with c-linkage")
{
    GIVEN("an object")
    {
        EmitterClass emitter;

        WHEN("object is connected to a function with c-linkage")
        {
            Object::connect(&emitter, &EmitterClass::signal, cIncreaseFunctionCounter);

            THEN("object connects to function")
            {
                functionCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    emitter.signal();

                    THEN("connected function is called")
                    {
                        REQUIRE(1 == functionCounter);
                    }
                }
            }
        }

        WHEN("object is connected to a function with c-linkage and a context object")
        {
            std::unique_ptr<Object> pCtxObject(new Object);
            Object::connect(&emitter, &EmitterClass::signal, pCtxObject.get(), cIncreaseFunctionCounter);

            THEN("object connects to function")
            {
                functionCounter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    emitter.signal();

                    THEN("connected function is called")
                    {
                        REQUIRE(1 == functionCounter);

                        AND_WHEN("signal is emitted again")
                        {
                            emitter.signal();

                            THEN("connected function is called one more time")
                            {
                                REQUIRE(2 == functionCounter);
                            }
                        }

                        AND_WHEN("context object is destryed")
                        {
                            pCtxObject.reset();

                            AND_WHEN("connected signal is emitted")
                            {
                                emitter.signal();

                                THEN("connected function is not called again")
                                {
                                    REQUIRE(1 == functionCounter);
                                }
                            }
                        }
                    }
                }

                AND_WHEN("context object is destryed")
                {
                    pCtxObject.reset();

                    functionCounter = 0;

                    AND_WHEN("connected signal is emitted")
                    {
                        emitter.signal();

                        THEN("connected function is not called")
                        {
                            REQUIRE(0 == functionCounter);
                        }
                    }
                }
            }
        }
    }
}


struct StaticMethod
{
    static int counter;
    static void increaseCounter() {++counter;}
};

int StaticMethod::counter = 0;


SCENARIO("Object connects to static methods")
{
    GIVEN("an object")
    {
        EmitterClass emitter;

        WHEN("object is connected to static method")
        {
            Object::connect(&emitter, &EmitterClass::signal, &StaticMethod::increaseCounter);

            THEN("object connects to function")
            {
                StaticMethod::counter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    emitter.signal();

                    THEN("connected function is called")
                    {
                        REQUIRE(1 == StaticMethod::counter);
                    }
                }
            }
        }

        WHEN("object is connected to a static method and a context object")
        {
            std::unique_ptr<Object> pCtxObject(new Object);
            Object::connect(&emitter, &EmitterClass::signal, pCtxObject.get(), &StaticMethod::increaseCounter);

            THEN("object connects to function")
            {
                StaticMethod::counter = 0;

                AND_WHEN("connected signal is emitted")
                {
                    emitter.signal();

                    THEN("connected function is called")
                    {
                        REQUIRE(1 == StaticMethod::counter);

                        AND_WHEN("signal is emitted again")
                        {
                            emitter.signal();

                            THEN("connected function is called one more time")
                            {
                                REQUIRE(2 == StaticMethod::counter);
                            }
                        }

                        AND_WHEN("context object is destryed")
                        {
                            pCtxObject.reset();

                            AND_WHEN("connected signal is emitted")
                            {
                                emitter.signal();

                                THEN("connected function is not called again")
                                {
                                    REQUIRE(1 == StaticMethod::counter);
                                }
                            }
                        }
                    }
                }

                AND_WHEN("context object is destryed")
                {
                    pCtxObject.reset();

                    StaticMethod::counter = 0;

                    AND_WHEN("connected signal is emitted")
                    {
                        emitter.signal();

                        THEN("connected function is not called")
                        {
                            REQUIRE(0 == StaticMethod::counter);
                        }
                    }
                }
            }
        }
    }
}


namespace Test::Object
{

class MoreThan10ArgsObject : public Kourier::Object
{
KOURIER_OBJECT(Test::Object::MoreThan10ArgsObject)
public:
    MoreThan10ArgsObject() = default;
    ~MoreThan10ArgsObject() override = default;
    void largeMethod(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12) {largeSum = (a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12);}
    Kourier::Signal largeSignal(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12);
    static void largeStaticFunction(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12) {largeSum = (a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12);}
    static int largeSum;
};

Kourier::Signal MoreThan10ArgsObject::largeSignal(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12) KOURIER_SIGNAL(&MoreThan10ArgsObject::largeSignal, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)
int MoreThan10ArgsObject::largeSum = 0;

static void largeArgsFunction(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12) {MoreThan10ArgsObject::largeSum = (a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12);}
extern "C"
{
static void cLargeArgsFunction(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12) {MoreThan10ArgsObject::largeSum = (a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12);}
}

}


SCENARIO("Object supports connections with more than 10 args")
{
    GIVEN("a object with methods with more than 10 args")
    {
        MoreThan10ArgsObject object;

        WHEN("a a connection to a method involving more than 10 args is established")
        {
            Object::connect(&object, &MoreThan10ArgsObject::largeSignal, &object, &MoreThan10ArgsObject::largeMethod);

            AND_WHEN("the signal is emitted")
            {
                MoreThan10ArgsObject::largeSum = 0;

                object.largeSignal(1,2,3,4,5,6,7,8,9,10,11,12);

                THEN("connected slot gets called")
                {
                    REQUIRE(78 == MoreThan10ArgsObject::largeSum);
                }
            }
        }

        WHEN("a a connection to a static method involving more than 10 args is established")
        {
            Object::connect(&object, &MoreThan10ArgsObject::largeSignal, &MoreThan10ArgsObject::largeStaticFunction);

            AND_WHEN("the signal is emitted")
            {
                MoreThan10ArgsObject::largeSum = 0;

                object.largeSignal(1,2,3,4,5,6,7,8,9,10,11,12);

                THEN("connected slot gets called")
                {
                    REQUIRE(78 == MoreThan10ArgsObject::largeSum);
                }
            }
        }

        WHEN("a a connection to a function involving more than 10 args is established")
        {
            Object::connect(&object, &MoreThan10ArgsObject::largeSignal, &largeArgsFunction);

            AND_WHEN("the signal is emitted")
            {
                MoreThan10ArgsObject::largeSum = 0;

                object.largeSignal(1,2,3,4,5,6,7,8,9,10,11,12);

                THEN("connected slot gets called")
                {
                    REQUIRE(78 == MoreThan10ArgsObject::largeSum);
                }
            }
        }

        WHEN("a a connection to a c-linkage function involving more than 10 args is established")
        {
            Object::connect(&object, &MoreThan10ArgsObject::largeSignal, &cLargeArgsFunction);

            AND_WHEN("the signal is emitted")
            {
                MoreThan10ArgsObject::largeSum = 0;

                object.largeSignal(1,2,3,4,5,6,7,8,9,10,11,12);

                THEN("connected slot gets called")
                {
                    REQUIRE(78 == MoreThan10ArgsObject::largeSum);
                }
            }
        }

        WHEN("a a connection to a lambda that does not capture involving more than 10 args is established")
        {
            Object::connect(&object, &MoreThan10ArgsObject::largeSignal, [](int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12){MoreThan10ArgsObject::largeSum = (a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12);});

            AND_WHEN("the signal is emitted")
            {
                MoreThan10ArgsObject::largeSum = 0;

                object.largeSignal(1,2,3,4,5,6,7,8,9,10,11,12);

                THEN("connected slot gets called")
                {
                    REQUIRE(78 == MoreThan10ArgsObject::largeSum);
                }
            }
        }

        WHEN("a a connection to a lambda that captures involving more than 10 args is established")
        {
            auto *pInt = &MoreThan10ArgsObject::largeSum;
            Object::connect(&object, &MoreThan10ArgsObject::largeSignal, [pInt](int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12){*pInt = (a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12);});

            AND_WHEN("the signal is emitted")
            {
                MoreThan10ArgsObject::largeSum = 0;

                object.largeSignal(1,2,3,4,5,6,7,8,9,10,11,12);

                THEN("connected slot gets called")
                {
                    REQUIRE(78 == MoreThan10ArgsObject::largeSum);
                }
            }
        }
    }
}


namespace Test::Object
{

class OverloadedMethods : public Kourier::Object
{
KOURIER_OBJECT(Test::Object::OverloadedMethods)
public:
    OverloadedMethods() = default;
    ~OverloadedMethods() override = default;
    Kourier::Signal signal(int a1);
    Kourier::Signal signal(int a1, int a2);
    void method(int a1) {a = a1; b = 0;}
    void method(int a1, int a2) {a = a1; b = a2;}
    int a = 0;
    int b = 0;
};

Kourier::Signal OverloadedMethods::signal(int a1) KOURIER_SIGNAL(qOverload<int>(&OverloadedMethods::signal), a1)
Kourier::Signal OverloadedMethods::signal(int a1, int a2) KOURIER_SIGNAL_OVERLOAD(&OverloadedMethods::signal, (int, int), a1, a2)

}


SCENARIO("Object supports overloaded methods")
{
    GIVEN("overloaded signals connected to overloaded methods")
    {
        OverloadedMethods object;
        Object::connect(&object, qOverload<int>(&OverloadedMethods::signal), &object, qOverload<int>(&OverloadedMethods::method));
        Object::connect(&object, qOverload<int,int>(&OverloadedMethods::signal), &object, qOverload<int,int>(&OverloadedMethods::method));

        WHEN("signal with one int is emitted")
        {
            object.a = 0;
            object.b = 0;
            const int emittedA = 5;
            object.signal(emittedA);

            THEN("corresponding slot gets called")
            {
                REQUIRE(emittedA == object.a);
                REQUIRE(0 == object.b);

                AND_WHEN("signal with two ints is emitted")
                {
                    const int emittedA = 15;
                    const int emittedB = 3;
                    object.signal(emittedA, emittedB);

                    THEN("corresponding slot gets called")
                    {
                        REQUIRE(emittedA == object.a);
                        REQUIRE(emittedB == object.b);
                    }
                }
            }
        }

        WHEN("signal with two ints is emitted")
        {
            object.a = 0;
            object.b = 0;
            const int emittedA = 5;
            const int emittedB = 3;
            object.signal(emittedA, emittedB);

            THEN("corresponding slot gets called")
            {
                REQUIRE(emittedA == object.a);
                REQUIRE(emittedB == object.b);

                AND_WHEN("signal with one int is emitted")
                {
                    const int emittedA = 15;
                    object.signal(emittedA);

                    THEN("corresponding slot gets called")
                    {
                        REQUIRE(emittedA == object.a);
                        REQUIRE(0 == object.b);
                    }
                }
            }
        }
    }
}


class Emitter : public QObject
{
Q_OBJECT
public:
    Emitter() = default;
    ~Emitter() override = default;
signals:
    void mySignal(int value);
};

class Receiver : public QObject
{
Q_OBJECT
public:
    Receiver() = default;
    ~Receiver() override = default;
    int value() const {return m_value;}
    void setValue(int value) {m_value = value;}
public slots:
    void mySlot(int value) {m_value = value;}

private:
    int m_value = 0;
};


SCENARIO("Object is faster than Qt's signal and slots")
{
    Emitter qEmitter;
    Receiver qReceiver;
    QObject::connect(&qEmitter, &Emitter::mySignal, &qReceiver, &Receiver::mySlot);
    REQUIRE(0 == qReceiver.value());
    const int repetitionCount = 1;
    EmitterClass emitter;
    TestObject object;
    Object::connect(&emitter, &EmitterClass::intSignal, &object, &TestObject::setCounterInt);
    REQUIRE(0 == object.value());
    QElapsedTimer timer;
    timer.start();
    for (auto i = 0; i < repetitionCount; ++i)
    {
        const int value = 5;
        emit qEmitter.mySignal(value);
        if (value == qReceiver.value())
            qReceiver.setValue(0);
        else
            qFatal("Failed");
    }
    const auto msElapsedForQt = timer.elapsed();
    WARN(QByteArray("Elapsed Time for Qt: ").append(QByteArray::number(msElapsedForQt).append("ms.")));
    timer.start();
    for (auto i = 0; i < repetitionCount; ++i)
    {
        const int value = 5;
        emitter.intSignal(value);
        if (value == object.value())
            object.setCounterInt(0);
        else
            qFatal("Failed");
    }
    const auto msElapsedForKourier = timer.elapsed();
    WARN(QByteArray("Elapsed Time for Kourier: ").append(QByteArray::number(msElapsedForKourier).append("ms.")));
    WARN(QByteArray("Kourier is ").append(QByteArray::number(msElapsedForQt/double(msElapsedForKourier)).append("x faster than Qt.")));
}


namespace Test::Object
{

class TestReceiver : public Kourier::Object
{
KOURIER_OBJECT(Test::Object::TestReceiver)
public:
    ~TestReceiver() override = default;
    void setValue(int value) {m_value = value;}

private:
    int m_value = 0;
};

}


#include <malloc.h>
SCENARIO("Object's signal-slot connection memory consumption")
{
    GIVEN("Kourier-based emitters and receivers")
    {
        // Creating
        constexpr size_t count = 1;
        std::vector<EmitterClass> emitters(count);
        emitters.shrink_to_fit();
        // malloc_trim(0);
        // raise(SIGTRAP);
        std::vector<TestReceiver> receivers(count);
        receivers.shrink_to_fit();
        // malloc_trim(0);
        // raise(SIGTRAP);
        // Connecting
        for (auto i = 0; i < count; ++i)
            Object::connect(&emitters[i], &EmitterClass::intSignal, &receivers[i], &TestReceiver::setValue);
        bool done = true;
        // malloc_trim(0);
        // raise(SIGTRAP);
    }
}


SCENARIO("Qt's signal-slot connection memory consumption")
{
    GIVEN("Qt-based emitters and receivers")
    {
        // Creating
        constexpr size_t count = 1;
        std::vector<Emitter> emitters(count);
        emitters.shrink_to_fit();
        // malloc_trim(0);
        // raise(SIGTRAP);
        std::vector<Receiver> receivers(count);
        receivers.shrink_to_fit();
        // malloc_trim(0);
        // raise(SIGTRAP);
        // Connecting
        for (auto i = 0; i < count; ++i)
            QObject::connect(&emitters[i], &Emitter::mySignal, &receivers[i], &Receiver::mySlot);
        bool done = true;
        // malloc_trim(0);
        // raise(SIGTRAP);
    }
}


class Emitter6 : public QObject
{
Q_OBJECT
public:
    Emitter6() = default;
    ~Emitter6() override = default;
signals:
    void mySignal1(int value);
    void mySignal2(int value);
    void mySignal3(int value);
    void mySignal4(int value);
    void mySignal5(int value);
    void mySignal6(int value);
};

class Receiver6 : public QObject
{
Q_OBJECT
public:
    Receiver6() = default;
    ~Receiver6() override = default;
    int value() const {return m_value;}
    void setValue(int value) {m_value = value;}
public slots:
    void mySlot1(int value) {m_value = value;}
    void mySlot2(int value) {m_value = value;}
    void mySlot3(int value) {m_value = value;}
    void mySlot4(int value) {m_value = value;}
    void mySlot5(int value) {m_value = value;}
    void mySlot6(int value) {m_value = value;}

private:
    int m_value = 0;
};


namespace Test::Object
{

class EmitterClass6 : public Kourier::Object
{
KOURIER_OBJECT(Test::Object::EmitterClass6)
public:
    Kourier::Signal mySignal1(int value);
    Kourier::Signal mySignal2(int value);
    Kourier::Signal mySignal3(int value);
    Kourier::Signal mySignal4(int value);
    Kourier::Signal mySignal5(int value);
    Kourier::Signal mySignal6(int value);
};

Kourier::Signal EmitterClass6::mySignal1(int value) KOURIER_SIGNAL(&EmitterClass6::mySignal1, value)
Kourier::Signal EmitterClass6::mySignal2(int value) KOURIER_SIGNAL(&EmitterClass6::mySignal2, value)
Kourier::Signal EmitterClass6::mySignal3(int value) KOURIER_SIGNAL(&EmitterClass6::mySignal3, value)
Kourier::Signal EmitterClass6::mySignal4(int value) KOURIER_SIGNAL(&EmitterClass6::mySignal4, value)
Kourier::Signal EmitterClass6::mySignal5(int value) KOURIER_SIGNAL(&EmitterClass6::mySignal5, value)
Kourier::Signal EmitterClass6::mySignal6(int value) KOURIER_SIGNAL(&EmitterClass6::mySignal6, value)

}


SCENARIO("Object's multiple signal-slot connections memory consumption")
{
    GIVEN("Kourier-based emitters and receivers")
    {
        // Creating
        constexpr size_t count = 1;
        std::vector<EmitterClass6> emitters(count);
        emitters.shrink_to_fit();
        // malloc_trim(0);
        // raise(SIGTRAP);
        std::vector<TestReceiver> receivers(count);
        receivers.shrink_to_fit();
        // malloc_trim(0);
        // raise(SIGTRAP);
        // Connecting
        for (auto i = 0; i < count; ++i)
        {
            Object::connect(&emitters[i], &EmitterClass6::mySignal1, &receivers[i], &TestReceiver::setValue);
            Object::connect(&emitters[i], &EmitterClass6::mySignal2, &receivers[i], &TestReceiver::setValue);
            Object::connect(&emitters[i], &EmitterClass6::mySignal3, &receivers[i], &TestReceiver::setValue);
            Object::connect(&emitters[i], &EmitterClass6::mySignal4, &receivers[i], &TestReceiver::setValue);
            Object::connect(&emitters[i], &EmitterClass6::mySignal5, &receivers[i], &TestReceiver::setValue);
            Object::connect(&emitters[i], &EmitterClass6::mySignal6, &receivers[i], &TestReceiver::setValue);
        }
        bool done = true;
        // malloc_trim(0);
        // raise(SIGTRAP);
    }
}


SCENARIO("Qt's multiple signal-slot connections memory consumption")
{
    GIVEN("Qt-based emitters and receivers")
    {
        // Creating
        constexpr size_t count = 1;
        std::vector<Emitter6> emitters(count);
        emitters.shrink_to_fit();
        // malloc_trim(0);
        // raise(SIGTRAP);
        std::vector<Receiver6> receivers(count);
        receivers.shrink_to_fit();
        // malloc_trim(0);
        // raise(SIGTRAP);
        // Connecting
        for (auto i = 0; i < count; ++i)
        {
            QObject::connect(&emitters[i], &Emitter6::mySignal1, &receivers[i], &Receiver6::mySlot1);
            QObject::connect(&emitters[i], &Emitter6::mySignal2, &receivers[i], &Receiver6::mySlot2);
            QObject::connect(&emitters[i], &Emitter6::mySignal3, &receivers[i], &Receiver6::mySlot3);
            QObject::connect(&emitters[i], &Emitter6::mySignal4, &receivers[i], &Receiver6::mySlot4);
            QObject::connect(&emitters[i], &Emitter6::mySignal5, &receivers[i], &Receiver6::mySlot5);
            QObject::connect(&emitters[i], &Emitter6::mySignal6, &receivers[i], &Receiver6::mySlot6);
        }
        bool done = true;
        // malloc_trim(0);
        // raise(SIGTRAP);
    }
}


namespace Test::Object
{

class ObjectDeleterTest : public Kourier::Object
{
KOURIER_OBJECT(Test::Object::ObjectDeleterTest);
public:
    ObjectDeleterTest() = default;
    ~ObjectDeleterTest() override {++m_deletedObjects;}
    static int deletedObjects() {return m_deletedObjects;}
    static void resetDeletedObjectsCount() {m_deletedObjects = 0;}

private:
    static int m_deletedObjects;
};

int ObjectDeleterTest::m_deletedObjects = 0;

}


SCENARIO("Objects can be scheduled for deletion")
{
    GIVEN("a number of objects to delete")
    {
        const auto objectsToDelete = GENERATE(AS(size_t), 0, 1, 3, 8);

        WHEN("objects are scheduled for deletion")
        {
            ObjectDeleterTest::resetDeletedObjectsCount();
            REQUIRE(ObjectDeleterTest::deletedObjects() == 0);
            for (auto i = 0; i < objectsToDelete; ++i)
                (new ObjectDeleterTest)->scheduleForDeletion();

            THEN("objects are deleted when control returns to the event loop")
            {
                REQUIRE(ObjectDeleterTest::deletedObjects() == 0);
                QCoreApplication::processEvents();
                REQUIRE(ObjectDeleterTest::deletedObjects() == objectsToDelete);
            }
        }
    }
}

#include "Object.spec.moc"
