#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

template <typename KeyType>
inline std::string KeyStrRepr(const KeyType& /*key*/) {
    return "[key type printing not supported]";
}

template <>
inline std::string KeyStrRepr(const std::string& key) {
    return key;
}

enum RegistryPriority {
    REGISTRY_FALLBACK = 1,
    REGISTRY_DEFAULT = 2,
    REGISTRY_PREFERRED = 3,
};

template <class SrcType, class ObjectPtrType, class... Args>
class Registry {
public:
    typedef std::function<ObjectPtrType(Args...)> Creator;

    Registry(bool warning = true) : registry_(), priority_(), terminate_(true), warning_(warning) {}

    void Register(const SrcType& key, Creator creator, const RegistryPriority priority = REGISTRY_DEFAULT) {
        std::cerr << __FILE__ << ":" << __LINE__ << " in Register key = " << key << std::endl;
        std::lock_guard<std::mutex> lock(register_mutex_);
        // The if statement below is essentially the same as the following line:
        // CHECK_EQ(registry_.count(key), 0) << "Key " << key
        //                                   << " registered twice.";
        // However, CHECK_EQ depends on google logging, and since registration is
        // carried out at static initialization time, we do not want to have an
        // explicit dependency on glog's initialization function.
        if (registry_.count(key) != 0) {
            auto cur_priority = priority_[key];
            if (priority > cur_priority) {
#ifdef DEBUG
                std::string warn_msg = "Overwriting already registered item for key " + KeyStrRepr(key);
                fprintf(stderr, "%s\n", warn_msg.c_str());
#endif
                registry_[key] = creator;
                priority_[key] = priority;
            } else if (priority == cur_priority) {
                std::string err_msg = "Key already registered with the same priority: " + KeyStrRepr(key);
                fprintf(stderr, "%s\n", err_msg.c_str());
                if (terminate_) {
                    std::exit(1);
                } else {
                    throw std::runtime_error(err_msg);
                }
            } else if (warning_) {
                std::string warn_msg = "Higher priority item already registered, skipping registration of " + KeyStrRepr(key);
                fprintf(stderr, "%s\n", warn_msg.c_str());
            }
        } else {
            registry_[key] = creator;
            priority_[key] = priority;
        }
    }

    void Register(const SrcType& key, Creator creator, const std::string& help_msg, const RegistryPriority priority = REGISTRY_DEFAULT) {
        std::cerr << __FILE__ << ":" << __LINE__ << " in Register key = " << key << std::endl;
        Register(key, creator, priority);
        help_message_[key] = help_msg;
    }

    inline bool Has(const SrcType& key) { return (registry_.count(key) != 0); }

    ObjectPtrType Create(const SrcType& key, Args... args) {
        std::cerr << __FILE__ << ":" << __LINE__ << " key = " << key << std::endl;
        if (registry_.count(key) == 0) {
            // Returns nullptr if the key is not registered.
            return nullptr;
        }
        return registry_[key](args...);
    }

    /**
     * Returns the keys currently registered as a std::vector.
     */
    std::vector<SrcType> Keys() const {
        std::vector<SrcType> keys;
        for (const auto& it : registry_) {
            keys.push_back(it.first);
        }
        return keys;
    }

    inline const std::unordered_map<SrcType, std::string>& HelpMessage() const { return help_message_; }

    const char* HelpMessage(const SrcType& key) const {
        auto it = help_message_.find(key);
        if (it == help_message_.end()) {
            return nullptr;
        }
        return it->second.c_str();
    }

    // Used for testing, if terminate is unset, Registry throws instead of
    // calling std::exit
    void SetTerminate(bool terminate) { terminate_ = terminate; }

private:
    std::unordered_map<SrcType, Creator> registry_;
    std::unordered_map<SrcType, RegistryPriority> priority_;
    bool terminate_;
    const bool warning_;
    std::unordered_map<SrcType, std::string> help_message_;
    std::mutex register_mutex_;

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
};

template <class SrcType, class ObjectPtrType, class... Args>
class Registerer {
public:
    explicit Registerer(const SrcType& key, Registry<SrcType, ObjectPtrType, Args...>* registry,
                        typename Registry<SrcType, ObjectPtrType, Args...>::Creator creator, const std::string& help_msg = "") {
        registry->Register(key, creator, help_msg);
    }

    explicit Registerer(const SrcType& key, const RegistryPriority priority, Registry<SrcType, ObjectPtrType, Args...>* registry,
                        typename Registry<SrcType, ObjectPtrType, Args...>::Creator creator, const std::string& help_msg = "") {
        registry->Register(key, creator, help_msg, priority);
    }

    template <class DerivedType>
    static ObjectPtrType DefaultCreator(Args... args) {
        return ObjectPtrType(new DerivedType(args...));
    }
};
