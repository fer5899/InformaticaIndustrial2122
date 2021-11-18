// -------------------------------------------------------------------
// Ejemplo de contol de un pulsador mediante polling e interrupciones
// -------------------------------------------------------------------

int boton_flash=0;       // GPIO0 = boton flash
int estado_int=HIGH;     // por defecto HIGH (PULLUP). Cuando se pulsa se pone a LOW

volatile unsigned long inicio_pulsacion = 0;
volatile bool print_inicio = false;
bool print_hecho = false;
volatile unsigned long ahora;


/*------------------  RTI  --------------------*/
// Rutina de Tratamiento de la Interrupcion (RTI)
ICACHE_RAM_ATTR void RTI() {
  int lectura=digitalRead(boton_flash);
  ahora= millis();
  // descomentar para eliminar rebotes
  if(lectura==estado_int || ahora-inicio_pulsacion<50) return; // filtro rebotes 50ms
  
  if(lectura==LOW)
  { 
   estado_int=LOW;
   print_inicio = true;
   inicio_pulsacion = ahora;
  }
  else
  {
   estado_int=HIGH;
   
  }
  
}

/*------------------ SETUP --------------------*/
void setup() {
  pinMode(boton_flash, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.println();
  // descomentar para activar interrupción
  attachInterrupt(digitalPinToInterrupt(boton_flash), RTI, CHANGE);
  Serial.println("Interrupcion instalada...");
  
  Serial.println("Pulsador listo...");
}

/*------------------- LOOP --------------------*/
void loop() {
  
  if ( print_inicio == true && print_hecho == false ) 
  {
   Serial.print("Inicio de la pulsación: ");
   Serial.println(ahora);
   print_hecho = true;
  }
  
  if (estado_int == HIGH && print_inicio == true) {
    Serial.print("Fin de la pulsación, duración: ");
    Serial.print(ahora - inicio_pulsacion);
    Serial.println(" ms)");
    print_inicio= false;
    print_hecho = false;
  }
  
  delay(10);
}
