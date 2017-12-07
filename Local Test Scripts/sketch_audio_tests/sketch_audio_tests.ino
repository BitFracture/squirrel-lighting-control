const int AUDIO_IN = A0;

const int VALUES_LEN = 5;
int values[VALUES_LEN];
int valuesTotal = 0;
int valuesNext = 0;


void setup() {
  for (int i = 0; i < VALUES_LEN; i++)
    values[i] = 0;
    
  pinMode(AUDIO_IN, INPUT);
  Serial.begin(9600);
}

const int sampleWindow = 5;
int sample = 0;

void loop() {
   unsigned long startMillis= millis();  // Start of sample window
   unsigned int peakToPeak = 0;   // peak-to-peak level
 
   unsigned int signalMax = 0;
   unsigned int signalMin = 1024;
 
   // collect data for 50 mS
   while (millis() - startMillis < sampleWindow)
   {
      sample = analogRead(AUDIO_IN);
      if (sample < 1024)  // toss out spurious readings
      {
         if (sample > signalMax)
         {
            signalMax = sample;  // save just the max levels
         }
         else if (sample < signalMin)
         {
            signalMin = sample;  // save just the min levels
         }
      }
   }
   peakToPeak = signalMax - signalMin;  // max - min = peak-peak amplitude
   double volts = (peakToPeak * 5.0) / 1024;  // convert to volts

  valuesNext = (valuesNext + 1) % VALUES_LEN;
  valuesTotal -= values[valuesNext];
  values[valuesNext] = volts * 100;
  valuesTotal += values[valuesNext];
  
  for (int voltsCount = int(valuesTotal / VALUES_LEN); voltsCount > 0; voltsCount--) {
    Serial.print("*");
  }
  
  //delay(1);
  Serial.println();
}
