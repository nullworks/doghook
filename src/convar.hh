#include "platform.hh"

#include <cfloat>
#include <climits>
#include <cstdlib>
#include <string>

// Using a tagged class system allows us to avoid the overhead
// of trying to dynamic_cast to each type and seeing if it works
// and then discarding that when we try the next

// Whilst it is more clumsy - it is faster.

// This will differ from the source version as we will split different
// Convar types into their own templated class so that you can static_cast
// to it once you know what you are dealing with.

namespace sdk {
class ConCommandBase;
class IConVar;
} // namespace sdk

enum class ConvarType {
    Bool,
    Int,
    Float,
    String,
    Enum,

    // Impossible...
    None = -1,
};

class ConvarBase {
    // Convar list
    static const ConvarBase *head;

    ConvarBase const *next;
    ConvarBase const *parent;

    char       internal_name[128];
    ConvarType t;

    sdk::ConCommandBase *tf_convar;

    bool init_complete;

    static void tf_convar_changed(sdk::IConVar *convar, const char *old_string, float old_float);

public:
    ConvarBase(const char *name, ConvarType type, const ConvarBase *parent);
    virtual ~ConvarBase();

    // virtual functions - so you dont have to check type, cast and then call
    virtual bool        from_string(const char *str) = 0;
    virtual const char *to_string() const            = 0;

    auto name() const { return internal_name; }
    auto type() const { return t; }

    class Convar_Range {
    public:
        class Iterator {
            const ConvarBase *current;

        public:
            Iterator() : current(nullptr) {}
            explicit Iterator(const ConvarBase *b) : current(b) {}

            auto &operator++() {
                current = current->next;
                return *this;
            }

            auto operator*() const {
                assert(current);
                return current;
            }

            auto operator==(const Iterator &b) const { return current == b.current; }
            auto operator!=(const Iterator &b) const { return !(*this == b); }
        };

        auto begin() const { return Iterator(head); }

        auto end() const { return Iterator(nullptr); }
    };

    static auto get_range() { return Convar_Range(); }

    static auto init_all() -> void;
};

template <typename T>
class Convar;

template <>
class Convar<bool> : public ConvarBase {
    bool value;

public:
    Convar(const char *name, const ConvarBase *parent) : ConvarBase(name, ConvarType::Bool, parent), value(false) {}

    Convar(const char *name, bool value, const ConvarBase *parent) : Convar(name, parent) {
        this->value = value;
    }

    bool from_string(const char *str) override final {
        assert(str);

        if (_stricmp(str, "false") == 0) {
            value = false;
            return false;
        } else if (_stricmp(str, "true") == 0) {
            value = true;
            return false;
        }
        value = atoi(str);

        return false;
    }

    const char *to_string() const override final {
        return value ? "true" : "false";
    }

    operator bool() const { return value; }

    auto operator=(bool v) {
        value = v;
        return *this;
    }
};

template <>
class Convar<int> : public ConvarBase {

    int value;

    // min + max values (INT_MAX == no value)
    int min_value;
    int max_value;

public:
    static constexpr int no_value = INT_MAX;

    Convar(const char *name, const ConvarBase *parent) : ConvarBase(name, ConvarType::Int, parent),
                                                         value(0), min_value(no_value), max_value(no_value) {}

    // use no_value if you want their to be no min/max
    Convar(const char *name, int value, int min_value, int max_value, const ConvarBase *parent) : Convar(name, parent) {
        this->value     = value;
        this->min_value = min_value;
        this->max_value = max_value;
    }

    bool from_string(const char *str) override {
        assert(str);

        auto new_value = atoi(str);

        if (min_value != no_value) {
            if (new_value < min_value) {
                value = min_value;
                return true;
            }
        }
        if (max_value != no_value) {
            if (new_value > max_value) {
                value = max_value;
                return true;
            }
        }

        value = new_value;

        return false;
    }

    const char *to_string() const override {
        static u32  cur_index = 0;
        static char temp[20][8];

        cur_index += 1;

#ifdef _DEBUG
        memset(temp[cur_index], 0, sizeof(temp));
#endif

#if doghook_platform_windows()
        _itoa_s(value, temp[cur_index], 10);
#else
        sprintf(temp[cur_index], "%d", value);
#endif
        return temp[cur_index];
    }

    operator int() const { return value; }

    auto operator=(int v) {
        value = v;
        return *this;
    }
};

template <>
class Convar<float> : public ConvarBase {

    float value;

    // min + max values (INT_MAX == no value)
    float min_value;
    float max_value;

public:
    static constexpr float no_value = FLT_MAX;

    Convar(const char *name, const ConvarBase *parent) : ConvarBase(name, ConvarType::Float, parent),
                                                         value(0), min_value(no_value), max_value(no_value) {}

    // use no_value if you want their to be no min/max
    Convar(const char *name, float value, float min_value, float max_value, const ConvarBase *parent) : Convar(name, parent) {
        this->value     = value;
        this->min_value = min_value;
        this->max_value = max_value;
    }

    bool from_string(const char *str) override {
        assert(str);

        auto new_value = static_cast<float>(atof(str));

        if (min_value != no_value) {
            if (value < min_value) {
                value = min_value;
                return true;
            }
        }
        if (max_value != no_value) {
            if (value > max_value) {
                value = min_value;
                return true;
            }
        }

        value = new_value;

        return false;
    }

    const char *to_string() const override {
        static u32  cur_index = 0;
        static char temp[20][8];

        cur_index += 1;

#ifdef _DEBUG
        memset(temp[cur_index], 0, sizeof(temp));
#endif

        // TODO: this is clumsy
        return std::to_string(value).c_str();
    }

    operator float() const { return value; }

    auto operator=(float v) {
        value = v;
        return *this;
    }
};

template <>
class Convar<char *> : public ConvarBase {
    char *value;

public:
    Convar(const char *name, const ConvarBase *parent) : ConvarBase(name, ConvarType::String, parent),
                                                         value(nullptr) {}

    Convar(const char *name, const char *value, const ConvarBase *parent) : Convar(name, parent) {
        auto size   = strlen(value) + 1;
        this->value = new char[size];
#if doghook_platform_windows()
        strcpy_s(this->value, size, value);
#elif doghook_platform_linux()
        strncpy(this->value, value, size);
#endif
    }

    ~Convar() {
        if (value != nullptr) {
            delete[] value;
            value = nullptr;
        }
    }

    bool from_string(const char *str) override {
        if (value != nullptr) delete[] value;

        auto size = strlen(str) + 1;
        value     = new char[size];

#if doghook_platform_windows()
        strcpy_s(this->value, size, str);
#elif doghook_platform_linux()
        strncpy(this->value, size, str);
#endif

        return false;
    }

    const char *to_string() const override {
        return value;
    }
};
