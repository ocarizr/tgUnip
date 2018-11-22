//Autor: Rafael José Mota Ocariz
//Este código tem como propósito coletar os dados dos sensores durante os ensaios
//Data: 21/05/2017

#include <Wire.h>
#include <memorysaver.h>
#include <UTFT.h>
#include <URTouch.h>
#include <URTouchCD.h>

// São utilizadas variáveis globais pois o hardware possui limitações de memória RAM
// o que impossibilita o uso excessivo de variáveis locais

// Declaração da fonte que será usada na interface gráfica.
extern uint8_t BigFont[];

UTFT myGLCD(SSD1289,38,39,40,41);               // Inicia uma instancia da impressão de interface gráfica na tela
URTouch myTouch(6,5,4,3,2);

// Endereço de comunicação dos sensores
const int MPU=0x68;                             // Endereço de transmissão para os sensores

int AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;          // Variáveis dos sensores
int x1, x2, y1, y2;                             // Dados de coordenadas para desenhar na tela LCD
bool bStop;                                     // Define quando para o ensaio

void setup()
{
  // Inicializa comunicação seria com velocidade 9600bits/s
  Serial.begin(9600);

  // Inicializa comunicação com sensores
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  
  // randomSeed(analogRead(0));

  // Inicializa o LCD e define a fonte de texto que será utilizada
  myGLCD.InitLCD(LANDSCAPE);
  myGLCD.setFont(BigFont);

  // Inicializa o sistema Touch
  myTouch.InitTouch(LANDSCAPE);
  myTouch.setPrecision(PREC_MEDIUM);
  
  // Inicializa as variáveis
  AcX = 0;
  AcY = 0;
  AcZ = 0;
  Tmp = 0;
  GyX = 0;
  GyY = 0;
  GyZ = 0;
  
  x1 = 0;
  x2 = 0;
  y1 = 0;
  y2 = 0;
  
  bStop = false;
}

void loop()
{
  myGLCD.clrScr();                                // Limpa a tela
  myGLCD.fillScr(VGA_WHITE);                      // Define o plano de fundo para branco

  myGLCD.setColor(VGA_BLUE);                      // Define a cor do objeto desenhado na tela
  myGLCD.setBackColor(VGA_BLUE);                  // Define a cor de base do texto escrito

  // Definir coordenadas de desenho do retangulo
  x1 = myGLCD.getDisplayXSize()/10;
  x2 = myGLCD.getDisplayXSize()*9/10;
  y1 = myGLCD.getDisplayYSize()*3/10;
  y2 = myGLCD.getDisplayYSize()*7/10;
  myGLCD.drawRect(x1, y1, x2, y2);                // Desenha o retangulo na tela

  myGLCD.setColor(255,130,0);                     // Define cor do texto
  if (bStop == false)
  {
    myGLCD.print(
      "Aquisitando dados",
      (myGLCD.getDisplayXSize()/2)-135,
      (myGLCD.getDisplayYSize()/2)-(myGLCD.getFontYsize()/2));  // Imprime o texto definido no centro da tela
  } else 
  {
    myGLCD.print(
      "Aquisicao de dados finalizada",
      (myGLCD.getDisplayXSize()/2)-135,
      (myGLCD.getDisplayYSize()/2)-(myGLCD.getFontYsize()/2));  // Imprime o texto definido no centro da tela
  }

  while(bStop == false)
  {
    Wire.beginTransmission(MPU);
    Wire.write(0x3B);
    Wire.endTransmission(false);

    Wire.requestFrom(MPU, 14, true);

    AcX = Wire.read()<<8|Wire.read();
    AcY = Wire.read()<<8|Wire.read();
    AcZ = Wire.read()<<8|Wire.read();
    Tmp = Wire.read()<<8|Wire.read();
    GyX = Wire.read()<<8|Wire.read();
    GyY = Wire.read()<<8|Wire.read();
    GyZ = Wire.read()<<8|Wire.read();

    Serial.print(AcX); Serial.print(",");
    Serial.print(AcY); Serial.print(",");
    Serial.print(AcZ); Serial.print(",");
    Serial.print(Tmp); Serial.print(",");
    Serial.print(GyX); Serial.print(",");
    Serial.print(GyY); Serial.print(",");
    Serial.print(GyZ); Serial.print("\n");

    if(myTouch.dataAvailable())
    {
      myTouch.read();
      bStop = true;
    }
  }
}
