// Pins
#define PIN_MOVE_COMPLETE 2
#define PIN_PULSE 3
#define PIN_SUPPLY_VOLTAGE A7

// Settings
#define PULSE_TRAIN_WIDTH_MS 1000
#define SUPPLY_VOLTAGE_CUTOFF_V 11.0
#define NO_FILTER_SLOTS 5

// Logic
unsigned long _time;
int _current_pos = 1;
volatile bool _is_moving = false;

// Constants
const String I_Strings[] = {
  "Smikkelsen_FilterWheel_1.0", 
  "FW3.1.5", 
  "P", 
  "S/N:001", 
  "Max Speed 400", 
  "Jitter 1", 
  "PX Offset ",
  "Threshold 1", 
  "FilterSlots 5", 
  "Pulse Width 4950uS"
  };

// Pulse widths per filter position (µs)
const int PULSE_WIDTHS[NO_FILTER_SLOTS] = {
  500,   // Position 1
  800,   // Position 2
  1100,  // Position 3
  1400,  // Position 4
  1700   // Position 5
};



void setup()
{
  Serial.begin(9600);
  
  // Set pinmodes
  pinMode(PIN_MOVE_COMPLETE, INPUT_PULLUP);
  pinMode(PIN_PULSE, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_MOVE_COMPLETE), MovePinInterrupt, CHANGE);
  
  _time = millis();
  
  MoveToPos(_current_pos);
}

void loop()
{
  if (Serial.available() > 0) 
  {
    // Msg. id and value
    char one = 0;
    char two = 0;
    delay(10);

    // Sanitize input
    if (!(Serial.peek() > 32))
    {
      Serial.read();
    }
    else if (Serial.available() == 1)
    {
      Serial.read();
    }

    // Msg. correct shape 
    else
    {
      // Read bytes
      one = Serial.read();
      two = Serial.read();

      // Check msg. id is char, value is numeric
      if (!(isDigit(one)) && isDigit(two))
      {
         HandleSerial(one, two);
      } 

      // Invalid. Clean buffer and ignore
      else
      {
         SerialFlush();
      }
    }       
  }
}

void MoveToPos(int pos)
{
  // Pos in legal range?
  if (pos <= 0 || pos > NO_FILTER_SLOTS)
  {
    SendSerial("Requested position outside range [1; " + String(NO_FILTER_SLOTS) + "]: " + String(pos));
    return;
  }

  // Already at position?
  if (pos == _current_pos)
  {
    SendSerial("Already at position: " + String(pos));
    return;
  }

  // Are we already moving
  if (_is_moving)
  {
    SendSerial("Wheel is already moving!");
    return;
  }

  // TODO: Check wheel 12V supply is OK -> Will cause EKOS to crash on connection without 12V...
  // float voltage = GetSupplyVoltage();
  // if (voltage < SUPPLY_VOLTAGE_CUTOFF_V)
  // {
  //   SendSerial("Supply voltage too low: " + String(voltage));
  //   return;
  // }

  // All seems ok:
  int pos_result = SendPulseTrain(pos);

  // Wait until pos is reached, and then update _current_pos
  while (_is_moving) delay(1);
  _current_pos = pos_result;
}

float GetSupplyVoltage()
{
  return (15.0/1023.0)*analogRead(PIN_SUPPLY_VOLTAGE);
}

void MovePinInterrupt()
{
  if (digitalRead(PIN_MOVE_COMPLETE) == 0)
    _is_moving = true;
  else
    _is_moving = false;
}

int SendPulseTrain(int pos)
{
  /* SBIG datasheet:
    To position the filter you need to produce a 1 second burst of a TTL level (5V), high-going
    pulse train of period 18ms (55 Hz) on pin 2 (relative to grund on pin 5). The desired
    position is programmed by varying the width of the high-going pulse while preserving the
    pulse period (high time plus low time) of 18ms.
  */

  // Send pulsetrain
  int pw = GetPulseWidth(pos);
  unsigned long start_pulse = millis();
  while (millis() - start_pulse < PULSE_TRAIN_WIDTH_MS)
  {
    digitalWrite(PIN_PULSE, HIGH);
    delayMicroseconds(pw);
    digitalWrite(PIN_PULSE, LOW);
    delayMicroseconds(10000); // maxvalue is 16383...
    delayMicroseconds(8000-pw);
  }

  return pos;
}

int GetPulseWidth(int pos)
{
  if (pos < 1 || pos > NO_FILTER_SLOTS)
    return 0; // error

  return PULSE_WIDTHS[pos - 1];
}

void SerialFlush()
{
  while (Serial.available())
  {
    Serial.read();
  }
}

void SendSerial (String message)
{
  Serial.println(message);
}

void(* resetFunc) (void) = 0;

int PositionWrapper(int pos)
{
  if (pos < 1)
      pos += NO_FILTER_SLOTS * ((1 - pos) / NO_FILTER_SLOTS + 1);

  return 1 + (pos - 1) % NO_FILTER_SLOTS;
}

void HandleSerial(char firstChar, char secondChar)
{
    // Value to int
    int number = int(secondChar - 48);

    switch (toupper(firstChar))
    {
      // Move backwards X slots (ASCOM)
      case 'B':
        MoveToPos(PositionWrapper(_current_pos - number));
        SendSerial("P" + String(_current_pos));
        break;
        

      // Move forwards X slots (ASCOM)
      case 'F':
        MoveToPos(PositionWrapper(_current_pos + number));
        SendSerial("P" + String(_current_pos));
        break;
        
        
      // Go to position (INDI)
      case 'G':
        MoveToPos(number);
        SendSerial("P" + String(_current_pos));
        break;


      // Information 
      case 'I':  
        // Current position
        if (number == 2) 
          SendSerial(I_Strings[number] + String(_current_pos));
        
        // Offset for current position [NOT IMPLEMENTED]
        else if (number == 6) 
          SendSerial("P" + String(_current_pos) + " Offset " + String(0));

        // Device information
        else
          SendSerial(I_Strings[number]);

        break;


      // Increase/decrease pulse width value [NOT IMPLEMENTED]
      case 'M':
      case 'N':
        SendSerial(I_Strings[9]);
        break;


      // Offset for position [NOT IMPLEMENTED]
      case 'O': 
        if (number > 0 && number <= NO_FILTER_SLOTS)
          SendSerial("P" + String(number) + " Offset " + String(0));
        break;


      // Reset commands
      case 'R':
        switch (number) 
        {
          // R0/R1 reset arduino
          case 0: 
          case 1:
            resetFunc();
            break;

          // R2 reset all offsets and move to home [NOT IMPLEMENTED]
          case 2: 
            MoveToPos(1); // TODO: Should we home?
            SendSerial("Calibration Removed");
            break;

          // R3 Send jitter value [NOT IMPLEMENTED]
          case 3: 
            SendSerial("Jitter 1");
            break;

          // R4 send motor speed [NOT IMPLEMENTED]
          case 4: 
            SendSerial("Max Speed 100%");
            break;
          
          //R5 send threshold [NOT IMPLEMENTED]
          case 5:
            SendSerial("Threshold 1");
            break;

          // R6 Save offsets to eeprom [NOT IMPLEMENTED]
          case 6: 
            break;
        }
        break;


      // Send speed [NOT IMPLEMENTED]
      case 'S': 
        SendSerial("Speed=100%");
        break;


      // Send raw sensor value [NOT IMPLEMENTED]
      case 'T': 
        SendSerial(String(1));
        break;


      // Send speed [NOT IMPLEMENTED]
      case '<': 
        SendSerial("Backwards 100%");
        break;


      // Send speed [NOT IMPLEMENTED]
      case '>':
        SendSerial("Forward 100%");
        break;


      // Send jitter [NOT IMPLEMENTED]
      case '[':
      case ']':
        SendSerial("Jitter 1");
        break;


      // Decrease/increase offset [NOT IMPLEMENTED]
      case '(':
      case ')':
        SendSerial("P" + String(_current_pos) + " Offset " + String(0));
        break;


      // Decrease/increase filter position threshold value [NOT IMPLEMENTED]
      case '{':
      case '}':
        SendSerial("Threshold 1");
        break;


      // Unknown
      default: 
        SendSerial("Command Unknown");
  }
}


