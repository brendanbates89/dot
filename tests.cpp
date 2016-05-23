#include <iostream>
#include "dot.h"

// Test convenience functions.
#define ASSERT_EQ(expr) {bool result = (expr); if (!result) {return false; }}
#define ASSERT_NOTEQ(expr) {bool result = !(expr); if (!result) {return false; }}
#define ASSERT_EXCEPT(expr) {bool thrown = false; try {expr;} catch(const Dot::ContainerException &e) {thrown = true; } if (!thrown) {return false; }}
#define ASSERT_NOEXCEPT(expr) {bool except = false; try {expr;} catch(const Dot::ContainerException &e) {except = true; } if (except) {return false; }}

enum Number {
    NUMBER_FIRST = 1,
    NUMBER_OTHER = 2
};

std::shared_ptr<Dot::Container> makeContainer() {
    return std::make_shared<Dot::Container>();
}

// Create a class holding the configuration for the object.
class NumberConfig {
public:
    int initialValue;
};

class StringConfig {
public:
    std::string initialValue;
};

// Specify a factory for creating new int objects.
class NumberFactory : public Dot::Factory<int, NumberConfig> {
    virtual int *generate(const NumberConfig &config) {
        return new int(config.initialValue);
    }
};

// Specify a factory for creating new int objects.
class StringFactory : public Dot::Factory<std::string, StringConfig> {
    virtual std::string *generate(const StringConfig &config) {
        return new std::string(config.initialValue);
    }
};

bool testDirect() {
    auto container = makeContainer();

    container->registerService(new int(1), NUMBER_FIRST);
    container->registerService(new int(2), NUMBER_OTHER);

    ASSERT_EQ(*(container->get<int>(NUMBER_FIRST)) == 1);
    ASSERT_EQ(*(container->get<int>(NUMBER_OTHER)) == 2);

    return true;
}

bool testFactory() {
    auto container = makeContainer();

    NumberConfig configFirst { .initialValue = 1 };
    NumberConfig configOther { .initialValue = 2 };

    container->registerFactory<char, Dot::EmptyConfig>([](const Dot::EmptyConfig &config) {
        return new char(3);
    });

    container->registerFactory<NumberFactory>();
    container->registerService<int>(configFirst, NUMBER_FIRST);
    container->registerService<int>(configOther, NUMBER_OTHER);
    container->registerService<char>();

    ASSERT_EQ(*(container->get<int>(NUMBER_FIRST)) == 1);
    ASSERT_EQ(*(container->get<int>(NUMBER_OTHER)) == 2);
    ASSERT_EQ(*(container->get<char>()) == 3);

    return true;
}

bool testErrors() {
    auto container = makeContainer();

    NumberConfig config { .initialValue = 1 };
    StringConfig configString { .initialValue = "Test" };

    // Create services and factories.
    container->registerFactory<NumberFactory>();
    container->registerService(new int(1), NUMBER_FIRST);
    container->registerService<int>(config, NUMBER_OTHER);

    // Register service twice.
    ASSERT_EXCEPT(
        container->registerService(new int(1), NUMBER_FIRST);
    );

    // Register factory twice.
    ASSERT_EXCEPT(
        container->registerFactory<NumberFactory>();
    );

    // Register via factory.
    ASSERT_EXCEPT(
        container->registerService<int>(config, NUMBER_OTHER);
    )

    // Register service with invalid factory.
    ASSERT_EXCEPT(
        container->registerService<std::string>(configString);
    )

    // Do this to avoid macro substitution errors.
    #define REGISTER_LAMBDA container->registerFactory<int, NumberConfig>

    // Register lambda factory.
    ASSERT_EXCEPT(
        REGISTER_LAMBDA([](const NumberConfig &config) {
            // Do nothing here.
            return new int(5);
        });
    )

    // Get object that doesn't exist.
    ASSERT_EXCEPT(
        container->get<std::string>();
    )

    // Generate object that doesn't exist.
    ASSERT_EXCEPT(
            container->generate<std::string>(configString);
    )

    // Unregister service that doesn't exist.
    ASSERT_EXCEPT(
            container->unregisterService<std::string>();
    )

    return true;
}

bool testUnregister() {
    auto container = makeContainer();

    container->registerService(new int(1));
    ASSERT_NOEXCEPT(
        container->get<int>();
    )

    container->unregisterService<int>();
    ASSERT_EXCEPT(
        container->get<int>();
    )

    // Test forced overwriting.
    container->registerService(new int(2), 0, true);
    ASSERT_EQ(*(container->get<int>()) == 2);

    return true;
}

bool testScope() {
    auto container = makeContainer();

    container->registerService(new int(1));
    {
        auto scope = container->getScope();
        scope->registerService(new char(2));

        // Test fetching of both items.
        ASSERT_NOEXCEPT(
            scope->get<int>();
            scope->get<char>();
        )

        // Overwrite the int.
        ASSERT_NOEXCEPT(
            scope->registerService(new int(3));
            *(scope->get<int>()) = 3;
        )

        ASSERT_EQ(*(scope->get<int>()) == 3);
    }

    // Scope exit - assert that char doesn't exist and int is still equal to 1.
    ASSERT_EXCEPT(
        container->get<char>();
    )

    ASSERT_EQ(*(container->get<int>()) == 1);

    return true;
}

bool testContainerAware() {
    class TestItem : public Dot::ContainerAware {
    public:
        TestItem() {
            makeScope();
        }

        TestItem(TestItem *other) :
                Dot::ContainerAware(other) {
            makeScope();
        }

        void createInt() {
            getContainer()->registerService(new int(1));
        }

        void testInt() {
            getContainer()->get<int>();
        }

        void createChar() {
            getContainer()->registerService(new char(2));
        }

        void testChar() {
            getContainer()->get<char>();
        }
    };

    TestItem testOuter;
    testOuter.createInt();
    {
        TestItem testInner(&testOuter);

        // TEst that the int exists in the inner service.
        ASSERT_NOEXCEPT(
            testInner.testInt();
        )

        // Create in the inner scope and test outer service.
        testInner.createChar();
        ASSERT_EXCEPT(
            testOuter.testChar();
        )

        // Attempt setting the int from inner.
        *(testInner.getContainer()->get<int>()) = 2;
    }

    ASSERT_EQ(*(testOuter.getContainer()->get<int>()) == 2);

    return true;
}

int main() {
    // List of available tests here.
    bool (*tests[])() = {
        &testDirect,
        &testFactory,
        &testErrors,
        &testUnregister,
        &testScope,
        &testContainerAware
    };

    // Iterate through all tests.
    bool pass = true;
    int i = 0;
    int testCount = sizeof(tests)/sizeof(void*);

    while(pass && i++ < testCount) {
        std::cout << "Running Test: " << std::to_string(i-1) << std::endl;
        pass = tests[i-1]();
    }

    if (!pass) {
        std::cout << "\tTest Failed!" << std::endl;
    } else {
        std::cout << std::endl << "All tests passed!" << std::endl;
    }

    return 0;
}