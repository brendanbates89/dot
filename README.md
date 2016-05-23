## Dot - Simple C++ Dependency Injection

**Dot** is a simple type-based Dependency Injection (DI) framework written in C++.  It utilizes modern C++ features such as smart pointers, RTTI, and template metaprogramming to create a dependency injection framework that is both robust at compile time and flexible at runtime.

### Features
- Single-header library with 365 lines of code (no linking required).
- Can optionally be used as a singleton.
- Scoping can be used to easily maintain object lifetime.
- Uses shared pointers for object storage - never worry about using freed objects.
- Exception-based error handling.

### Limitations
- Uses types for compile time checking, but falls back to runtime for container validation.
- No automatic injection - objects must be manually fetched from the container.

### Advantages of a DI Container

The primary advantage of using a DI container is to decouble object creation with object usage.  Another advantage in a language like C++ is avoiding using setters, getters, and constructor arguments to pass objects through.  Normally this means naming services with strings and returning non type-safe pointers.  Dot, however, is written type-safe and uses C++ runtime type information to fetch objects.

### Table of Contents
- [Getting Started](#getting-started)
    - [Direct Creation](#direct-creation)
    - [Retrieving a Stored Object](#retrieving)
    - [Type Multiplicity](#type-multiplicity)
    - [Factory Creation](#factory-creation)
    - [Convenience Macros](#macros)
- [Subclassing ContainerAware](#container-aware)
- [Error Handling](#error-handling)
- [Advanced Topics](#advanced-topics)
    - [Unregistering and Overwriting Services](#unregistering)
    - [Scoping](#scoping)
    - [Lambda Factories](#lambda)

## Getting Started <a name="getting-started"></a>

Through out this tutorial, we'll be using the singleton version of the container: `Dot::AppContainer`.  This can be accessed anywhere you include the `dot.h` file:

    #include <dot.h>
    
    int main() {
        auto container = Dot::AppContainer::getInstance();
        
        return 0;
    }
    
Dot uses two concepts fundamental concepts: factories and services.  A **service** is any object that is created or maintained by Dot.  A **factory** is a method to construct a service using configuration.  Let's start out by creating a simple service without a factory.

### Direct Creation <a name="direct-creation"></a>

A service can be created as a pointer and then given to Dot:

    int *number = new int(42);
    container->registerService(number);
    
After this point, the lifetime of the int is now managed by Dot using shared pointers.  That means we don't have to worry about deleting or storing the object.  

### Retrieving a Stored Object <a name="retrieving"></a>

To retrieve the object, we can use Dots `get()` method.  Dot is entirely type based, so the object is retrieved using the type:

    auto myNumber = container->get<int>();
    std::cout << *myNumber << std::endl; // Outputs '42'
    *myNumber = 123;  // Sets the number to '123'
    
### Type Multiplicity <a name="type-multiplicity"></a>

Dot facilitates the ability to store multiple objects of the same type.  In order to do this, you can assign a specific ID to the object being stored (using an enum, for example):

    enum Number {
        NUMBER_FIRST,
        NUMBER_OTHER
    }
    
    int *numberFirst = new int(42);
    int *numberOther = new int(123);
    
    container->registerService(numberFirst, NUMBER_FIRST);
    container->registerService(numberOther, NUMBER_OTHER);
    
When retrieving the objects, just pass in the same ID to retrieve them:

    auto first = container->get<int>(NUMBER_FIRST);
    std::cout << *first << std::endl; // Outputs '42'
    
### Factory Creation <a name="factory-creation"></a>

A factory can be used to automatically generate objects on the fly.  The only requirement for a factory is that it must subclass the `Dot::Factory` pure virtual class.  This is simple and only has a single `generate` method:
    
    // Create a class holding the configuration for the object.
    class NumberConfig {
    public:
        int initialValue;
    };
    
    // Specify a factory for creating new int objects.
    class NumberFactory : public Dot::Factory<int, NumberConfig> {
        virtual int *generate(const NumberConfig &config) {
            return new int(config.initialValue);
        }
    };
    
Once created, the factory and configuration class can be registered:

    container->registerFactory<NumberFactory>();
    
A service object can then be registered using a configuration: 

    NumberConfig configFirst { .initialValue = 42 };
    NumberConfig configOther { .initialValue = 123 };
    
    // Register directly to the container with the NUMBER_FIRST id.
    container->registerService<int>(configFirst, NUMBER_FIRST);
    auto first = container->get<int>(NUMBER_FIRST);
    
The `generate()` method can also be used to generate an object without persisting it to the container.  This is useful for a factory which is often used to create objects that don't need to be saved:

    auto throwaway = container->generate<int>(configOther);

## Convenience Macros <a name="macros"></a>

A few convenience macros are provided for setting member variables using the Dot container.  If using the default singleton container:

- `DOT_INJECT(type, member)`  will inject the given type into the given member.
- `DOT_INJET_ID(type, id, member)` will inject the given type (with id) into the given member.

Examples:

    class TestClass {
    public:
        TestClass() {
            DOT_INJECT(int, _intMember);
            DOT_INJECT_ID(int, 1, _intMember);
        }
    private:
        std::shared_ptr<int> _intMember;        
    }

If using a container other than the singleton (such as a scoped container), the container can be passed in as an additional argument using the `DOT_INJECT_FROM` and `DOT_INJECT_ID_FROM` macros:

    DOT_INJECT_FROM(int, _intMember, _myContainer);
    DOT_INJECT_ID_FROM(int, 1, _intMember, _myContainer);

## Subclassing ContainerAware <a name="container-aware"></a>

By using the `ContainerAware` class, you can easily make existing classes able to have access to the container.  It can be subclassed and used to retrieve and scope the container:

    class TestFoo : public Dot::ContainerAware {
    public:
       TestFoo() {
            makeScope();
            
            // Do stuff with the container here.
            auto int = getContainer()->get<char>();
       }
       
       TestFoo(TestFoo *other) :
                Dot::ContainerAware(other) {
            makeScope();
            
            // Do stuff with the container here.
            auto int = getContainer()->get<int>();
       }
    }
    
    TestFoo first;
    TestFoo second(&first);
    
In the above example, a `TestFoo` class is created which can either accept the global singleton scope, or accept the scope of another `TestFoo`.  In both cases, the object creates an inner scope to do work in.

## Error Handling <a name="error-handling"></a>

Error handling in this library is fairly simple.  All API functions will throw a `Dot::ContainerException` with a string description of the problem.  Most of the time problems will stem from either:

- Services which have not yet been registered.
- Bad casts from passing invalid types.

Using exceptions, it becomes pretty easy to trace back the issues from where they were thrown.  If an exception wasn't thrown, it can be assumed that the object being registered or retrieved is valid.  This is because of C++ `dynamic_cast` along with Runtime Type Information guarentees that all types being converted internally are accurate.
    
## Advanced Topics <a name="advanced"></a>

The features covered here are not advanced *per-se*, but are extras to help with lifetime management.

### Unregistering and Overwriting <a name="unregistering"></a>

Services can be manually unregistered for deletion:

    container->unregisterService<int>(NUMBER_FIRST);
    
Services can also be overwritten if they need to be regenerated.  This is accomplished by passing `true` as the third argument, `allowOverwrite`:

    container->registerService<int>(configFirst, NUMBER_FIRST, true);
    
### Container Scoping <a name="scoping"></a>

It is not always desirable to have objects in the global container scope, especially for more complicated applications.  Furthermore, long running applications may find that a DI container may actually be a memory leak, as resources are created and abandonded.  This can be controlled using scopes.

A **Container Scope** is basically a child container based off of a parent container.  This child container inherits all services from the parent container.  Any new services created under this scope are not available to the parent, and all services will be released when the scope is exited.

A scope can always be created from a container, including the singleton, using the `getScope()` method.  Here is a full example showing scoping:

    auto container = Dot::Container::getInstance();
    
    // This will be available to the child scope.
    container->registerService(new int(5));
    
    // Create a scope in brackets so it destructs.
    {
        // Get new scope and register a char.
        auto scope = container->getScope();
        scope->registerService(new char(10));
        
        // Both of these are valid:
        scope->get<int>();
        scope->get<char>();
    }
    
    // At this point, the char is now destroyed.  Only the int is valid.
    scope->get<int>();
    
At this time, factories are registered globally.  This means registering a factory in a child scope will make it visible at the parent scope.

### Lambda Factories <a name="lambda"></a>

Instead of creating a class for a factory, a lambda function can be used in place.  The following syntax shows the proper format to register a lambda factory:

    container->registerFactory<int, NumberConfig>([](const NumberConfig &config){
        return new int(config.initialValue);
    });
    
As always with lambda values, care should be take with how values are passed in.  Once registered, this lambda may be called at any time when needed to construct the given type.