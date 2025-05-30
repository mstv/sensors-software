/*****************************************************************
 * Value with 24h min/max                                        *
 *****************************************************************/
class MinMaxBase
{
public:
    static void update_last_values();

    MinMaxBase(const char* name)
    : _name(name)
    {
        s_instances.emplace_back(*this);
    }

    virtual void append_to(String& json) = 0;
    virtual void update_last_values(const bool is_noon) = 0;

    const char* const _name;

    static std::vector<std::reference_wrapper<MinMaxBase>> s_instances;
};

std::vector<std::reference_wrapper<MinMaxBase>> MinMaxBase::s_instances;

template <typename T = float, int InvalidValue = -1, bool UpdateLastMinAtNoon = false, bool UpdateLastMaxAtNoon = false>
class MinMax : public MinMaxBase
{
public:
    MinMax(const char* name)
    : MinMaxBase{name}
    { }

    MinMax operator=(const T value)
    {
        _value = value;

        if (_value != _invalidValue)
        {
            if (_currentMin > _value || _currentMin == InvalidValue)
            {
                _currentMin = _value;
            }
            if (_currentMax < _value || _currentMax == InvalidValue)
            {
                _currentMax = _value;
            }
        }

        return *this;
    }

    operator const T&() const
    {
        return _value;
    }

    operator T&()
    {
        return _value;
    }

    virtual void append_to(String& json) override
    {
        json += String(F("{\"value_type\":\"")) + _name + F("\",\"value\":\"") + String(_value)
                + F("\",\"min\":[") + String(_currentMin) + ',' + String(_lastMin)
                + F("],\"max\":[") + String(_currentMax) + ',' + String(_lastMax)
                + F("]},");
    }

    virtual void update_last_values(const bool is_noon) override
    {
        if (UpdateLastMinAtNoon == is_noon)
        {
            _lastMin = _currentMin;
            _currentMin = _value;
        }
        if (UpdateLastMaxAtNoon == is_noon)
        {
            _lastMax = _currentMax;
            _currentMax = _value;
        }
    }

    const T _invalidValue{static_cast<T>(InvalidValue)};
    T _value{_invalidValue};
    T _currentMin{_invalidValue};
    T _currentMax{_invalidValue};
    T _lastMin{_invalidValue};
    T _lastMax{_invalidValue};
};

typedef MinMax<>                                                                       FloatValue;
typedef MinMax<float, -128, /*UpdateLastMinAtNoon*/true>                               TemperatureValue;
typedef MinMax<float,   -1, /*UpdateLastMinAtNoon*/false, /*UpdateLastMaxAtNoon*/true> HumidityValue;

extern uint8_t sntp_time_set;

inline void MinMaxBase::update_last_values()
{
    if (sntp_time_set == 0)
        return;

    const time_t now = time(nullptr);
    static time_t s_next_update = now;
    if (now < s_next_update)
        return;

    const time_t hours = now / 3600;
    const time_t hour = hours % 24;
    const time_t hour12 = hour % 12;
    s_next_update = (hours - (hours % 12) + 12) * 3600;

    if (hour12 == 0)
    {
        const bool is_noon = hour != 0;
        for (MinMaxBase& value : s_instances)
        {
            value.update_last_values(is_noon);
        }
    }
}
