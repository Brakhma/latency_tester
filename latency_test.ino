// Настройка пинов
const int photoPin = A0;    // Пин фоторезистора
const int ledPin = 12;      // Пин светодиода

// Настройки измерений
const unsigned int measurementCount = 25; // Количество измерений
unsigned long measurements[measurementCount]; // Массив измерений

// Переменные состояния
volatile unsigned int current = 0;     // Текущее измерение
unsigned int command = 0;              // Команда с Serial
unsigned long threshold = 10;          // Пороговое значение
unsigned long min_val = 0;             // Минимальное значение
unsigned long max_val = 0;             // Максимальное значение
unsigned long average = 0;             // Среднее значение

// Настройки временных интервалов
const unsigned int randomDelayMin = 1000;
const unsigned int randomDelayMax = 2000;
bool running = false;

// Функция калибровки
void calibration() {
  digitalWrite(ledPin, LOW);
  delay(500);
  min_val = analogRead(photoPin);

  digitalWrite(ledPin, HIGH);
  delay(500);
  max_val = analogRead(photoPin);
  digitalWrite(ledPin, LOW);

  // Автоматический расчет порога
  threshold = min_val + (max_val - min_val) * 0.2; // 20% от диапазона

  Serial.print(F("Минимум: "));
  Serial.print(min_val);
  Serial.print(F(" Максимум: "));
  Serial.print(max_val);
  Serial.print(F(" Порог: "));
  Serial.println(threshold);
}

// Вывод результатов с защитой от переполнения UART
void printResults() {
  average = 0;
  max_val = 0;  
  min_val = measurements[0];

  Serial.println(F("\n##### НАЧАЛО #####"));
  
  for(uint8_t i = 0; i < measurementCount; i++) {
    if(measurements[i] > max_val) max_val = measurements[i];
    if(measurements[i] < min_val) min_val = measurements[i];
    average += measurements[i];
    
    // Ждем готовности UART перед отправкой
    while(Serial.availableForWrite() < 20) { /* ждем места в буфере */ }
    
    if(i < 10) Serial.print('0');
    Serial.print(i);
    Serial.print(F(":\t"));
    Serial.print(measurements[i]);
    Serial.print(F("мкс\t("));
    Serial.print(measurements[i] / 1000);
    Serial.println(F("мс)"));
  }

  // Вывод статистики
  while(Serial.availableForWrite() < 30) {}
  Serial.println(F("#################"));
  Serial.print(F("Среднее:\t"));
  Serial.print(average / measurementCount);
  Serial.print(F("мкс\t("));
  Serial.print(average / measurementCount / 1000);
  Serial.println(F("мс)"));

  while(Serial.availableForWrite() < 30) {}
  Serial.print(F("Минимум:\t"));
  Serial.print(min_val);
  Serial.print(F("мкс\t("));
  Serial.print(min_val / 1000);
  Serial.println(F("мс)"));

  while(Serial.availableForWrite() < 30) {}
  Serial.print(F("Максимум:\t"));
  Serial.print(max_val);
  Serial.print(F("мкс\t("));
  Serial.print(max_val / 1000);
  Serial.println(F("мс)"));
  
  Serial.println(F("##### КОНЕЦ #####"));
}

void setup() {
  // Настройка АЦП
  analogReference(DEFAULT);
  
  pinMode(photoPin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Инициализация Serial
  Serial.begin(115200);
  while(!Serial) {;} // Ждем инициализации UART
  
  Serial.println(F("\n#################"));
  Serial.println(F("1 - калибровка"));
  Serial.println(F("2 - измерения"));
  Serial.println(F("6 - тест производительности"));
  Serial.println(F("#################"));
}

// Обработка команд с Serial
void processSerial() {
  while(Serial.available() > 0) {
    command = Serial.parseInt();
    
    // Очищаем буфер от лишних данных
    while(Serial.available() > 0) Serial.read();
    
    switch(command) {
      case 1: // Калибровка
        Serial.println(F("Выполняется калибровка..."));
        calibration();
        break;
        
      case 2: // Измерения
        Serial.println(F("Выполняются измерения..."));
        running = true;
        current = 0;
        break;
        
      case 6: // Тест производительности
        Serial.println(F("Тест производительности..."));
        performanceTest();
        break;
    }
  }
}

// Тест производительности
void performanceTest() {
  unsigned long start, end;
  uint16_t dummy = 0;
  
  // Тест скорости АЦП
  start = micros();
  for(uint8_t i = 0; i < 100; i++) {
    dummy += analogRead(photoPin);
  }
  end = micros();
  
  Serial.print(F("Скорость АЦП: "));
  Serial.print((end-start)/100.0);
  Serial.println(F(" мкс на чтение"));
  
  // Тест скорости GPIO
  start = micros();
  for(uint8_t i = 0; i < 100; i++) {
    digitalWrite(ledPin, HIGH);
    digitalWrite(ledPin, LOW);
  }
  end = micros();
  
  Serial.print(F("Скорость GPIO: "));
  Serial.print((end-start)/200.0);
  Serial.println(F(" мкс на переключение"));
}

// Выполнение одного измерения
unsigned long takeMeasurement() {
  uint32_t start, end;
  
  do {
    digitalWrite(ledPin, HIGH);
    start = micros();
    
    // Ожидание порога с защитой от зависания
    while(analogRead(photoPin) < threshold) {
      if((micros() - start) > 100000) { // Защита от зависания (100мс)
        digitalWrite(ledPin, LOW);
        return 0;
      }
    }
    
    end = micros();
    digitalWrite(ledPin, LOW);
  } while(start > end); // Повторяем если было переполнение таймера

  return end - start;
}

void loop() {
  processSerial();

  while(running) {
    measurements[current] = takeMeasurement();
    current++;

    if(current >= measurementCount) {
      running = false;
      printResults();
      return;
    }

    delay(random(randomDelayMin, randomDelayMax));
  }
}
