#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <random>


// 1. СТРУКТУРЫ И ПЕРЕЧИСЛЕНИЯ ДЛЯ СОБЫТИЙ И СРЕДЫ


enum class EventType {
    TEMP_CRITICAL_LOW,      // 1. Критическое падение температуры
    TEMP_CRITICAL_HIGH,     // 2. Перегрев теплицы
    HUMIDITY_LOW,           // 3. Низкая влажность (засуха)
    TARGET_CLIMATE_REACHED, // 4. Оптимальный микроклимат достигнут
    DEVICE_STATE_CHANGED,   // 5. Изменение состояния устройства (вкл/выкл)
    MANUAL_OVERRIDE_ALERT,  // 6. Ручное вмешательство пользователя
    DEVICE_ADDED            // 7. Подключение нового устройства
};

struct Event {
    int step;
    EventType type;
    std::string source;
    std::string message;

    std::string getTypeString() const {
        switch (type) {
        case EventType::TEMP_CRITICAL_LOW:      return "[КРИТ. ХОЛОД]";
        case EventType::TEMP_CRITICAL_HIGH:     return "[ПЕРЕГРЕВ]";
        case EventType::HUMIDITY_LOW:           return "[ЗАСУХА]";
        case EventType::TARGET_CLIMATE_REACHED: return "[НОРМА]";
        case EventType::DEVICE_STATE_CHANGED:   return "[УСТРОЙСТВО]";
        case EventType::MANUAL_OVERRIDE_ALERT:  return "[РУЧНОЙ РЕЖИМ]";
        case EventType::DEVICE_ADDED:           return "[ПОДКЛЮЧЕНИЕ]";
        default:                                return "[СОБЫТИЕ]";
        }
    }
};

struct Environment {
    double temperature; // Температура воздуха (°C)
    double humidity;    // Влажность почвы/воздуха (%)
    int lightLevel;     // Освещенность (люкс)
};


// 2. ШАБЛОННЫЙ КЛАСС ЖУРНАЛА СОБЫТИЙ И ШАБЛОННАЯ ФУНКЦИЯ


template <typename T>
T clampValue(T val, T minVal, T maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

template <typename TEvent>
class EventLogger {
private:
    std::vector<TEvent> logs;
    bool printToConsole;

public:
    EventLogger(bool consoleOutput = true) : printToConsole(consoleOutput) {}
    ~EventLogger() {
        logs.clear();
    }

    void log(const TEvent& event) {
        logs.push_back(event);
        if (printToConsole) {
            std::cout << "  >> Шаг " << std::setw(2) << event.step << " "
                << std::left << std::setw(16) << event.getTypeString()
                << " | " << event.source << ": " << event.message << "\n";
        }
    }

    const std::vector<TEvent>& getLogs() const { return logs; }
    size_t getCount() const { return logs.size(); }

    void printFullHistory() const {
        std::cout << "\n==================== ЖУРНАЛ СОБЫТИЙ (" << logs.size() << " записей) ====================\n";
        for (const auto& e : logs) {
            std::cout << "Шаг " << std::setw(2) << e.step << " | "
                << std::left << std::setw(16) << e.getTypeString() << " | "
                << std::setw(20) << e.source << " | " << e.message << "\n";
        }
        std::cout << "========================================================================\n";
    }
};


// 3. ШАБЛОННЫЙ КЛАСС ДАТЧИКА (С ПЕРЕГРУЗКОЙ ОПЕРАТОРОВ)


template <typename T>
class Sensor {
private:
    std::string name;
    std::string unit;
    T currentValue;

public:
    Sensor(std::string sensorName, std::string unitMeasure, T initialVal)
        : name(sensorName), unit(unitMeasure), currentValue(initialVal) {
    }

    ~Sensor() {}

    // Геттеры и сеттеры (свойства компонента)
    T getValue() const { return currentValue; }
    void setValue(T val) { currentValue = val; }
    std::string getName() const { return name; }
    std::string getUnit() const { return unit; }

    // Перегрузка операторов сравнения для удобной проверки порогов
    bool operator<(const T& threshold) const { return currentValue < threshold; }
    bool operator>(const T& threshold) const { return currentValue > threshold; }
    bool operator==(const T& threshold) const { return currentValue == threshold; }

    // Перегрузка оператора присваивания значения
    Sensor<T>& operator=(const T& newVal) {
        currentValue = newVal;
        return *this;
    }

    // Перегрузка оператора потокового вывода <<
    friend std::ostream& operator<<(std::ostream& os, const Sensor<T>& sensor) {
        os << sensor.name << ": " << std::fixed << std::setprecision(1)
            << sensor.currentValue << " " << sensor.unit;
        return os;
    }
};


// 4. АБСТРАКТНЫЙ БАЗОВЫЙ КЛАСС УСТРОЙСТВА (ПОЛИМОРФИЗМ)


class Device {
protected:
    int id;
    std::string name;
    bool isActive;
    double powerWatts;

public:
    Device(int devId, std::string devName, double power)
        : id(devId), name(devName), isActive(false), powerWatts(power) {
    }

    virtual ~Device() {}

    // Чисто виртуальные методы (абстрактный класс)
    virtual void update(Environment& env, double targetTemp, double targetHum,
        int step, EventLogger<Event>& logger) = 0;
    virtual std::string getDeviceType() const = 0;

    // Методы получения и изменения данных (инкапсуляция)
    int getId() const { return id; }
    std::string getName() const { return name; }
    bool getStatus() const { return isActive; }
    double getPower() const { return isActive ? powerWatts : 0.0; }

    void setStatus(bool status, int step, EventLogger<Event>& logger) {
        if (isActive != status) {
            isActive = status;
            logger.log({
                step,
                EventType::DEVICE_STATE_CHANGED,
                name,
                std::string("Состояние изменено на: ") + (isActive ? "ВКЛ" : "ВЫКЛ")
                });
        }
    }
};


// 5. КОНКРЕТНЫЕ КЛАССЫ УСТРОЙСТВ (НАСЛЕДНИКИ DEVICE)


class ClimateController : public Device {
private:
    double heatingPower; // На сколько градусов поднимает за шаг
    double coolingPower; // На сколько градусов снижает за шаг

public:
    ClimateController(int id, std::string name, double power, double heatRate = 2.5, double coolRate = 2.0)
        : Device(id, name, power), heatingPower(heatRate), coolingPower(coolRate) {
    }

    ~ClimateController() override {}

    std::string getDeviceType() const override { return "Климат-контроль"; }

    void update(Environment& env, double targetTemp, double targetHum,
        int step, EventLogger<Event>& logger) override {
        const double tolerance = 1.0;

        if (env.temperature < targetTemp - tolerance) {
            if (!isActive) {
                logger.log({ step, EventType::TEMP_CRITICAL_LOW, name,
                            "Температура (" + std::to_string((int)env.temperature) + "°C) ниже нормы. Запуск обогрева." });
                setStatus(true, step, logger);
            }
            env.temperature += heatingPower;
        }
        else if (env.temperature > targetTemp + tolerance) {
            if (!isActive) {
                logger.log({ step, EventType::TEMP_CRITICAL_HIGH, name,
                            "Температура (" + std::to_string((int)env.temperature) + "°C) выше нормы. Запуск охлаждения." });
                setStatus(true, step, logger);
            }
            env.temperature -= coolingPower;
        }
        else {
            if (isActive) {
                logger.log({ step, EventType::TARGET_CLIMATE_REACHED, name,
                            "Целевая температура достигнута. Переход в режим ожидания." });
                setStatus(false, step, logger);
            }
        }
    }
};

class IrrigationSystem : public Device {
private:
    double wateringRate; // На сколько % поднимает влажность за шаг

public:
    IrrigationSystem(int id, std::string name, double power, double rate = 12.0)
        : Device(id, name, power), wateringRate(rate) {
    }

    ~IrrigationSystem() override {}

    std::string getDeviceType() const override { return "Система автополива"; }

    void update(Environment& env, double targetTemp, double targetHum,
        int step, EventLogger<Event>& logger) override {
        const double tolerance = 5.0;

        if (env.humidity < targetHum - tolerance) {
            if (!isActive) {
                logger.log({ step, EventType::HUMIDITY_LOW, name,
                            "Влажность (" + std::to_string((int)env.humidity) + "%) упала ниже порога. Включение полива." });
                setStatus(true, step, logger);
            }
            env.humidity = clampValue(env.humidity + wateringRate, 0.0, 100.0);
        }
        else if (env.humidity >= targetHum) {
            if (isActive) {
                logger.log({ step, EventType::TARGET_CLIMATE_REACHED, name,
                            "Оптимальная влажность восстановлена. Полив остановлен." });
                setStatus(false, step, logger);
            }
        }
    }
};

class LightingSystem : public Device {
private:
    int minLightThreshold;

public:
    LightingSystem(int id, std::string name, double power, int minLux = 400)
        : Device(id, name, power), minLightThreshold(minLux) {
    }

    ~LightingSystem() override {}

    std::string getDeviceType() const override { return "Фитоосвещение"; }

    void update(Environment& env, double targetTemp, double targetHum,
        int step, EventLogger<Event>& logger) override {
        if (env.lightLevel < minLightThreshold) {
            setStatus(true, step, logger);
            env.lightLevel += 300;
        }
        else if (env.lightLevel > minLightThreshold + 350) {
            setStatus(false, step, logger);
        }
    }
};


// 6. ГЛАВНЫЙ КЛАСС ТЕПЛИЦЫ (КОМПОЗИЦИЯ + АГРЕГАЦИЯ)


class SmartGreenhouse {
private:
    std::string title;
    Environment env;
    double targetTemperature;
    double targetHumidity;
    int currentStep;

    // КОМПОЗИЦИЯ: датчики и журнал событий являются неотъемлемой частью теплицы
    Sensor<double> tempSensor;
    Sensor<double> humiditySensor;
    Sensor<int> lightSensor;
    EventLogger<Event> logger;

    // АГРЕГАЦИЯ: устройства создаются извне и подключаются по указателям
    std::vector<Device*> devices;

public:
    SmartGreenhouse(std::string greenhouseTitle, double initTemp, double initHum, int initLight)
        : title(greenhouseTitle),
        env{ initTemp, initHum, initLight },
        targetTemperature(24.0),
        targetHumidity(65.0),
        currentStep(0),
        tempSensor("Датчик температуры", "°C", initTemp),
        humiditySensor("Датчик влажности", "%", initHum),
        lightSensor("Датчик света", "лк", initLight),
        logger(true) {
    }

    ~SmartGreenhouse() {
        // При разрушении теплицы очищаем список ссылок (сами внешние объекты не удаляем)
        devices.clear();
    }

    // Подключение внешнего устройства (Агрегация)
    void attachDevice(Device* device) {
        if (device != nullptr) {
            devices.push_back(device);
            logger.log({ currentStep, EventType::DEVICE_ADDED, title,
                        "Подключено устройство: " + device->getName() + " [" + device->getDeviceType() + "]" });
        }
    }

    // Ручное изменение целевых параметров (Событие ручного вмешательства)
    void setTargetClimate(double newTemp, double newHum) {
        targetTemperature = clampValue(newTemp, 10.0, 40.0);
        targetHumidity = clampValue(newHum, 20.0, 95.0);
        logger.log({ currentStep, EventType::MANUAL_OVERRIDE_ALERT, "Оператор",
                    "Заданы новые цели: T = " + std::to_string((int)targetTemperature) +
                    "°C, Влажность = " + std::to_string((int)targetHumidity) + "%" });
    }

    // Имитация одного шага (часа) работы теплицы
    void simulateStep() {
        currentStep++;

        // Естественное изменение внешней среды (имитация суток)
        double externalTempDrift = (currentStep % 24 >= 6 && currentStep % 24 <= 18) ? 1.4 : -1.8;
        env.temperature += externalTempDrift;
        env.humidity = clampValue(env.humidity - 4.5, 10.0, 100.0);
        env.lightLevel = (currentStep % 24 >= 7 && currentStep % 24 <= 19) ? 850 : 150;

        // Обновление показаний датчиков (использование перегруженного оператора =)
        tempSensor = env.temperature;
        humiditySensor = env.humidity;
        lightSensor = env.lightLevel;

        std::cout << "\n--- Шаг симуляции #" << currentStep << " (Время суток: " << (currentStep % 24) << ":00) ---\n";
        std::cout << "Показания до реакции: " << tempSensor << " | " << humiditySensor << " | " << lightSensor << "\n";

        // Полиморфный вызов логики всех подключенных устройств
        for (Device* dev : devices) {
            dev->update(env, targetTemperature, targetHumidity, currentStep, logger);
        }

        // Фиксация итоговых показателей после работы оборудования
        tempSensor = env.temperature;
        humiditySensor = env.humidity;
        lightSensor = env.lightLevel;
    }

    void printStatus() const {
        double totalPower = 0.0;
        std::cout << "\n================ ТЕКУЩИЙ СТАТУС: " << title << " ================\n";
        std::cout << "Целевой климат:  T = " << targetTemperature << " °C | Влажность = " << targetHumidity << " %\n";
        std::cout << "Текущие датчики: " << tempSensor << " | " << humiditySensor << " | " << lightSensor << "\n";
        std::cout << "Подключенные устройства (" << devices.size() << "):\n";
        for (const Device* dev : devices) {
            std::cout << "  - [ID:" << dev->getId() << "] " << std::left << std::setw(24) << dev->getName()
                << " | Статус: " << (dev->getStatus() ? "ВКЛ " : "ВЫКЛ")
                << " | Мощность: " << dev->getPower() << " Вт\n";
            totalPower += dev->getPower();
        }
        std::cout << "Суммарное текущее энергопотребление: " << totalPower << " Вт\n";
        std::cout << "====================================================================\n";
    }

    void showEventLog() const {
        logger.printFullHistory();
    }
};


// 7. ТОЧКА ВХОДА И ИНТЕРАКТИВНОЕ КОНСОЛЬНОЕ МЕНЮ

int main() {
    setlocale(LC_ALL, "Russian");

    // Создаем внешние устройства (демонстрация агрегации)
    ClimateController mainClimate(101, "Терморегулятор ТК-1", 1500.0, 2.8, 2.2);
    IrrigationSystem dripIrrigation(102, "Капельный полив КП-4", 120.0, 14.0);
    LightingSystem phytoLamp(103, "Фитолампа ФЛ-200", 200.0, 400);

    // Создаем теплицу
    SmartGreenhouse greenhouse("Умная теплица №1 (Томаты)", 19.0, 52.0, 300);

    // Подключаем устройства к теплице
    greenhouse.attachDevice(&mainClimate);
    greenhouse.attachDevice(&dripIrrigation);
    greenhouse.attachDevice(&phytoLamp);

    int choice = -1;
    while (choice != 0) {
        std::cout << "\n=== МЕНЮ УПРАВЛЕНИЯ УМНОЙ ТЕПЛИЦЕЙ ===\n";
        std::cout << "1. Выполнить 1 шаг симуляции (1 час)\n";
        std::cout << "2. Выполнить серию из 6 шагов симуляции\n";
        std::cout << "3. Показать текущее состояние датчиков и устройств\n";
        std::cout << "4. Изменить целевую температуру и влажность (ручной режим)\n";
        std::cout << "5. Вывести полный журнал событий\n";
        std::cout << "0. Выход\n";
        std::cout << "Выберите действие: ";

        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "Ошибка ввода. Введите число от 0 до 5.\n";
            continue;
        }

        switch (choice) {
        case 1:
            greenhouse.simulateStep();
            break;
        case 2:
            for (int i = 0; i < 6; ++i) {
                greenhouse.simulateStep();
            }
            break;
        case 3:
            greenhouse.printStatus();
            break;
        case 4: {
            double newT, newH;
            std::cout << "Введите новую целевую температуру (10..40 °C): ";
            std::cin >> newT;
            std::cout << "Введите новую целевую влажность (20..95 %): ";
            std::cin >> newH;
            greenhouse.setTargetClimate(newT, newH);
            break;
        }
        case 5:
            greenhouse.showEventLog();
            break;
        case 0:
            std::cout << "Завершение работы симулятора...\n";
            break;
        default:
            std::cout << "Неизвестная команда.\n";
            break;
        }
    }

    return 0;
}
