//Autor: Rafael José Mota Ocariz
//Este código tem como propósito calcular os angulos de variação da vertical e imprimir na tela para o usuário
//Data: 10/12/2018

#include <Wire.h>
#include <memorysaver.h>
#include <UTFT.h>
#include <URTouch.h>
#include <URTouchCD.h>
#include <math.h>

// Declaração da fonte que será usada na interface gráfica.
extern uint8_t BigFont[];

UTFT myGLCD(SSD1289,38,39,40,41);               // Inicia uma instancia da impressão de interface gráfica na tela
URTouch myTouch(6,5,4,3,2);                     // Inicia uma instancia do touch

// Endereço de comunicação dos sensores
const int MPU=0x68;                             // Endereço de transmissão para os sensores

float AcX[400], AcY[400], AcZ[400], Tmp, GyX, GyY, GyZ, Time;          // Variáveis dos sensores
float sumAcX = 0, sumAcY = 0, sumAcZ = 0, sumGyX = 0, sumGyY = 0, sumGyZ = 0;               // Soma das variáveis
int x1, x2, y1, y2;                             // Dados de coordenadas para desenhar na tela LCD
int btn1X1, btn1X2, btnY1, btnY2;               // Coordenadas do botão Medir
int btn2X1, btn2X2;                             // Coordenadas do botão Zerar

int i = 0;                                      // indice dos dados medidos no processo de medição

float medidoX = 0, medidoY = 0;                 // Os dados impressos ao usuário
float alfa, beta;                               // Os angulos de Euler calculados
float precision = 0.01f;                        // precisão cobrada dos cálculos

const float DPALLANX = 16.12f;
const float DPALLANY = 18.09f;
const float DPALLANZ = 38.31f;

void setup() {
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
  myTouch.setPrecision(PREC_HI);
}

void loop() {
  myGLCD.clrScr();                                 // Limpa a tela
  myGLCD.fillScr(210,210,210);                     // Define o plano de fundo para branco
  myGLCD.setBackColor(0,255,0);

  // Impressão do cabeçalho na tela
  myGLCD.setColor(0,255,0);
  myGLCD.fillRect(0,0,myGLCD.getDisplayXSize(), myGLCD.getDisplayYSize()/10);
  myGLCD.setColor(255,255,255);
  myGLCD.print("SDV - TCC Rafael", myGLCD.getDisplayXSize()/2 - 120, myGLCD.getDisplayYSize()/20-(myGLCD.getFontYsize()/2));

  // Impressão dos dados
  myGLCD.setBackColor(0,0,0);
  myGLCD.print("X", 50, (myGLCD.getDisplayYSize()/5)+myGLCD.getFontYsize());
  myGLCD.printNumF(medidoX, 2, 150, (myGLCD.getDisplayYSize()/5)+myGLCD.getFontYsize());
  myGLCD.print("Y", 50, (myGLCD.getDisplayYSize()*11/20)-myGLCD.getFontYsize());
  myGLCD.printNumF(medidoY, 2, 150, (myGLCD.getDisplayYSize()*11/20)-myGLCD.getFontYsize());

  // Imprime o botão Medir
  btn1X1 = myGLCD.getDisplayXSize()/20;
  btn1X2 = myGLCD.getDisplayXSize()*5/20;
  btnY1 = myGLCD.getDisplayYSize()*19/20;
  btnY2 = myGLCD.getDisplayYSize()*17/10;
  myGLCD.setColor(0,0,255);                                     // Define a cor do objeto desenhado na tela
  myGLCD.fillRect(btn1X1, btnY1, btn1X2, btnY2);                // Desenha o retangulo na tela
  myGLCD.setColor(80,150,255);
  myGLCD.drawRect(btn1X1, btnY1, btn1X2, btnY2);                // Desenha o retangulo na tela
  myGLCD.setColor(255,0,0);
  myGLCD.setBackColor(80,150,255);                              // Define a cor de base do texto escrito
  myGLCD.print("Medir", btn1X1 + 20, (myGLCD.getDisplayYSize()*18/20)-(myGLCD.getFontYsize()/2));  // Imprime o texto definido no centro da tela

  // Imprime o botão Zerar
  btn2X1 = myGLCD.getDisplayXSize()*13/20;
  btn2X2 = myGLCD.getDisplayXSize()*17/20;
  myGLCD.setColor(0,0,255);                                     // Define a cor do objeto desenhado na tela
  myGLCD.fillRect(btn2X1, btnY1, btn2X2, btnY2);                // Desenha o retangulo na tela
  myGLCD.setColor(80,150,255);
  myGLCD.drawRect(btn2X1, btnY1, btn2X2, btnY2);                // Desenha o retangulo na tela
  myGLCD.setColor(255,0,0);
  myGLCD.setBackColor(80,150,255);                              // Define a cor de base do texto escrito
  myGLCD.print("Zerar", btn2X1 + 20, (myGLCD.getDisplayYSize()*18/20)-(myGLCD.getFontYsize()/2));  // Imprime o texto definido no centro da tela

  while(!myTouch.dataAvailable())
  {
    i=0;
  }

  if(myTouch.dataAvailable())
  {
    myTouch.read();

    // Verifica se o usuário tocou em um botão    
    if (myTouch.getY() < 205 && myTouch.getY() > 170)
    {
      if (myTouch.getX() > 45 && myTouch.getX() < 135)
      {
        myGLCD.setColor(0,0,0);
        // Desenha o retangulo na tela para a barra de carregamento
        myGLCD.drawRect(btn1X1, myGLCD.getDisplayYSize()*15/20, btn1X2, myGLCD.getDisplayYSize()*13/20);
        while (i<400)
        {
          Wire.beginTransmission(MPU);
          Wire.write(0x3B);
          Wire.endTransmission(false);

          Wire.requestFrom(MPU, 14, true);

          AcX[i] = Wire.read()<<8|Wire.read();
          AcY[i] = Wire.read()<<8|Wire.read();
          AcZ[i] = Wire.read()<<8|Wire.read();
          Tmp = Wire.read()<<8|Wire.read();
          GyX = Wire.read()<<8|Wire.read();
          GyY = Wire.read()<<8|Wire.read();
          GyZ = Wire.read()<<8|Wire.read();
          Time = millis();

          sumAcX += AcX[i];
          sumAcY += AcY[i];
          sumAcZ += AcZ[i];
          
          myGLCD.setColor(100,255,100);
          myGLCD.fillRect(btn1X1, myGLCD.getDisplayYSize()*15/20, btn1X1 + (btn1X2-btn1X1)*i/1200, myGLCD.getDisplayYSize()*13/20);   // Desenha a barra de carregamento
          
          i++;     
        }
        
        i=0;
        sumAcX = (sumAcX/400);
        sumAcY = (sumAcY/400);
        sumAcZ = ((sumAcZ/400)-(-2540));
        
        while (i<400)
        {
          if (AcX[i] > (sumAcX + 2*DPALLANX))
          {
            AcX[i] = sumAcX + 2*DPALLANX;
          } else if (AcX[i] < (sumAcX - 2*DPALLANX))
          {
            AcX[i] = sumAcX - 2*DPALLANX;
          }

          if (AcY[i] > (sumAcY + 2*DPALLANY))
          {
            AcY[i] = sumAcY + 2*DPALLANY;
          } else if (AcY[i] < (sumAcY - 2*DPALLANY))
          {
            AcY[i] = sumAcY - 2*DPALLANY;
          }

          if (AcZ[i] > (sumAcZ + 2*DPALLANZ))
          {
            AcZ[i] = sumAcZ + 2*DPALLANZ;
          } else if (AcZ[i] < (sumAcZ - 2*DPALLANZ))
          {
            AcZ[i] = sumAcZ - 2*DPALLANZ;
          }
             
          myGLCD.setColor(100,255,100);
          myGLCD.fillRect(btn1X1, myGLCD.getDisplayYSize()*15/20, btn1X1 + (btn1X2-btn1X1)*(400+i)/1200, myGLCD.getDisplayYSize()*13/20);   // Desenha a barra de carregamento
          
          i++;     
        }
        i = 0;
        
        while (i<400)
        {
          sumAcX += AcX[i];
          sumAcY += AcY[i];
          sumAcZ += AcZ[i];

          myGLCD.setColor(100,255,100);
          myGLCD.fillRect(btn1X1, myGLCD.getDisplayYSize()*15/20, btn1X1 + (btn1X2-btn1X1)*(800+i)/1200, myGLCD.getDisplayYSize()*13/20);   // Desenha a barra de carregamento
          i++;
        }
        i = 0;

        sumAcX = (((sumAcX/400)-344.98f)/16400)*(-1);
        sumAcY = (((sumAcY/400)-(-2.917f))/16108)*(-1);
        sumAcZ = (((sumAcZ/400)-(-2565.7f))/16613)*(-1);

        alfa = atan(sumAcZ/sumAcX);
        beta = atan(-sumAcY/((sumAcX*cos(alfa))+(sumAcZ*sin(alfa))));

        alfa = alfa*57.2958f;           // Convertendo radianos para graus
        beta = beta*57.2958f;           // Convertendo radianos para graus

        medidoX = beta;
        medidoY = alfa;
        
      } else if(myTouch.getX() > 215 && myTouch.getX() < 300)
      {
        medidoX = 0;
        medidoY = 0;
      } 
   } 
 }
}
