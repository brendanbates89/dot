#ifndef DOT_LIBRARY_H
#define DOT_LIBRARY_H

#include <typeinfo>
#include <typeindex>
#include <memory>
#include <map>
#include <functional>
#include <mutex>

#define DOT_INJECT(type, member) member = Dot::AppContainer::getInstance()->get<type>()
#define DOT_INJECT_ID(type, id, member) member = Dot::AppContainer::getInstance()->get<type>(id)

#define DOT_INJECT_FROM(type, member, injector) blah
#define DOT_INJECT_ID_FROM(type, member, id, injector) blah

namespace Dot {

/**
 * Empty configuration object for generating simple objects without a configuration required.
 */
class EmptyConfig {

};

/**
 * Base factory class used for storing templated factories.
 */
class BaseFactory {
public:
    virtual ~BaseFactory() { }
};

/**
 * Base exception class used for catching container exceptions.
 */
class ContainerException : public std::runtime_error {
public:
    ContainerException(const std::string &what) :
            std::runtime_error(what) {
        
    }
};

/**
 * Abstract factory object, used for generating objects of the specified types using
 * a given configuration.
 */
template<typename Type, typename Config>
class Factory : public BaseFactory {
public:
    /**
     * Generates an object of the specified Type using the config object passed in.
     */
    virtual Type* generate(const Config &config) = 0;

    /**
     * Returns object-specific type info for the factory of the current type.
     */
    static std::type_index getTypeInfo() {
        return std::type_index(typeid(Type));
    }
};

/**
 * Basic version of a factory.  This uses the EmptyConfig configuration type
 * and will create an object using the default constructor.
 */
template<typename Type>
class BasicFactory : public Factory<Type, EmptyConfig> {
public:
    virtual Type *generate(const EmptyConfig &config)  {
        return new Type;
    };
};

template<typename Type, typename Config>
class LambdaFactory : public Factory<Type, Config> {
public:
    LambdaFactory(std::function<Type *(const Config &config)> lambda) :
            _lambda(lambda) {

    }

    virtual Type *generate(const Config &config)  {
        return _lambda(config);
    };

private:
    std::function<Type *(const Config &config)> _lambda;
};

/**
 *
 */
class Container : public std::enable_shared_from_this<Container> {
    class BaseObjectContainer {
    public:
        virtual ~BaseObjectContainer() { }
    };

    template<typename Type>
    class ObjectContainer : public BaseObjectContainer {
    public:
        virtual ~ObjectContainer() { }
        std::shared_ptr<Type> object;
    };

public:
    Container() :
            _factories(std::make_shared<std::map<std::type_index, std::shared_ptr<BaseFactory>>>()) {
    }

    virtual ~Container() {

    }

    std::shared_ptr<Container> getScope() {
        return std::shared_ptr<Container>(new Container(shared_from_this()));
    }

    template<typename Type>
    void registerService(Type* instance, int id = 0, bool allowOverwrite = false) throw(ContainerException) {
        std::lock_guard<std::recursive_mutex> locker(_mutex);

        std::type_index type = typeid(Type);
        std::string typeName(type.name());

        // Check if the given ID already exists.
        if(!allowOverwrite && _objects.count(type) && _objects[type].count(id)) {
            std::string message = "Service object for type \"" + typeName + "\" with id \"" + std::to_string(id) + "\" already exists in injector.";
            throw ContainerException(message.data());
        }

        std::shared_ptr<Type> object(instance);
        auto container = std::make_shared<ObjectContainer<Type>>();
        container->object = object;

        _objects[type][id] = container;
    }

    template<typename Type, typename Config>
    void registerService(Config config = EmptyConfig(), int id = 0, bool allowOverwrite = false) throw(ContainerException) {
        std::lock_guard<std::recursive_mutex> locker(_mutex);

        std::type_index type = typeid(Type);
        std::string typeName(type.name());

        // Check if the given type exists.
        if (!_factories->count(type)) {
            std::string message = "Factory for type \"" + typeName + "\" does not exist in injector.";
            throw ContainerException(message.data());
        }

        // Check if the given ID already exists.
        if(!allowOverwrite && _objects.count(type) && _objects[type].count(id)) {
            std::string message = "Service object for type \"" + typeName + "\" with id \"" + std::to_string(id) + "\" already exists in injector.";
            throw ContainerException(message.data());
        }

        // Fetch the factory and attempt to cast it.
        auto factory = (*_factories)[type];
        auto castFactory = std::dynamic_pointer_cast<Factory<Type, Config>>(factory);
        if (!castFactory) {
            std::string message = "Invalid cast when fetching factory for type \"" + typeName + "\".";
            throw ContainerException(message.data());
        }

        // Generate the actual object to store.
        auto object = std::shared_ptr<Type>(castFactory->generate(config));
        auto container = std::make_shared<ObjectContainer<Type>>();
        container->object = object;

        _objects[type][id] = container;
    }

    template<typename Type>
    void registerService(int id = 0, bool allowOverwrite = false) throw(ContainerException) {
        registerService<Type>(EmptyConfig(), id, allowOverwrite);
    }

    template<typename Factory>
    void registerFactory() throw(ContainerException) {
        std::lock_guard<std::recursive_mutex> locker(_mutex);

        std::type_index type = Factory::getTypeInfo();
        std::string typeName(type.name());
        
        if (_factories->count(type)) {
            std::string message = "Factory for type \"" + typeName + "\" already exists in injector.";
            throw ContainerException(message.data());
        }

        (*_factories)[type] = std::make_shared<Factory>();
    }

    template<typename Type, typename Config>
    void registerFactory(std::function<Type *(const Config &)> generator) {
        std::lock_guard<std::recursive_mutex> locker(_mutex);

        std::type_index type = typeid(Type);
        std::string typeName(type.name());

        if (_factories->count(type)) {
            std::string message = "Factory for type \"" + typeName + "\" already exists in injector.";
            throw ContainerException(message.data());
        }

        (*_factories)[type] = std::make_shared<LambdaFactory<Type, Config>>(generator);
    };

    template<typename Type>
    std::shared_ptr<Type> get(int id = 0) throw(ContainerException) {
        std::lock_guard<std::recursive_mutex> locker(_mutex);

        std::type_index type = typeid(Type);
        std::string typeName(type.name());

        if (!_objects.count(type) || !_objects[type].count(id)) {
            if (_parent) {
                return _parent->get<Type>(id);
            } else {
                std::string message = "Service object for type \"" + typeName + "\" with id \"" + std::to_string(id) +
                                      "\" doesn't exist in injector.";
                throw ContainerException(message.data());
            }
        }

        // Attempt to properly cast the container to the given type.
        auto container = _objects[type][id];
        auto castContainer = std::dynamic_pointer_cast<ObjectContainer<Type>>(container);
        if (!castContainer) {
            std::string message = "Invalid object cast during injector get.";
            throw ContainerException(message.data());
        }

        return castContainer->object;
    };

    template<typename Type, typename Config>
    std::shared_ptr<Type> generate(Config config) throw(ContainerException) {
        std::lock_guard<std::recursive_mutex> locker(_mutex);

        std::type_index type = typeid(Type);
        std::string typeName(type.name());

        // Check for a factory.
        if (!_factories->count(type)) {
            auto type = typeid(Type).name();
            throw ContainerException("Factory for type \"" + typeName + "\" does not exist in injector.");
        }

        auto factory = (*_factories)[type];
        auto castFactory = std::dynamic_pointer_cast<Factory<Type, Config>>(factory);
        if (!castFactory) {
            auto type = typeid(Type).name();
            throw ContainerException("Invalid cast when fetching factory for type \"" + typeName + "\".");
        }

        // Generate the object.
        return std::shared_ptr<Type>(castFactory->generate(config));
    };

    template<typename Type>
    void unregisterService(int id = 0) throw(ContainerException) {
        std::lock_guard<std::recursive_mutex> locker(_mutex);

        std::type_index type = typeid(Type);
        std::string typeName(type.name());

        if (!_objects.count(type) || !_objects[type].count(id)) {
            std::string message = "Service object for type \"" + typeName + "\" with id \"" + std::to_string(id) + "\" doesn't exist in injector.";
            throw ContainerException(message.data());
        }

        _objects[type].erase(id);
    }

private:
    std::shared_ptr<std::map<std::type_index, std::shared_ptr<BaseFactory>>> _factories;
    std::map<std::type_index, std::map<int, std::shared_ptr<BaseObjectContainer>>> _objects;
    std::shared_ptr<Container> _parent;
    std::recursive_mutex _mutex;

    Container(std::shared_ptr<Container> parent) :
            _parent(parent) {
        std::lock_guard<std::recursive_mutex> locker(_parent->_mutex);
        _factories = _parent->_factories;
    }

    /**
     * Check if this container
     */
    template<typename Type>
    bool contains(int id) {
        if (_objects.count(typeid(Type)) && _objects[typeid(Type)].count(id)) {
            return true;
        } else if (_parent) {
            return _parent->contains<Type>(id);
        }

        return false;
    }

};

class AppContainer : public Container {
public:
    static std::shared_ptr<AppContainer> getInstance() {
        static auto injector = std::shared_ptr<AppContainer>(new AppContainer);
        return injector;
    }

    virtual ~AppContainer() {

    }

private:
    AppContainer() {

    }

    AppContainer(AppContainer const&) = delete;
    void operator =(AppContainer const&) = delete;

};

class ContainerAware {
public:
    ContainerAware() :
            _container(AppContainer::getInstance()) {

    }

    ContainerAware(ContainerAware *other) :
            _container(other->_container) {

    }

    ContainerAware(std::shared_ptr<Container> container) :
            _container(container) {

    }

    void setContainer(std::shared_ptr<Container> container) {
        _container = container;
    }

    std::shared_ptr<Container> getContainer() {
        return _container;
    }

protected:
    std::shared_ptr<Container> _container;

    void makeScope() {
        _container = _container->getScope();
    }

};

}

#endif //DTECT_IT_COMMON_INJECTOR_H
