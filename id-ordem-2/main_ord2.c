#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define PWM_4V 3280
#define PWM_1_5V 1240
#define PWM_1V 800
#define CTE_DE_SAIDA 4.8875855e-3
#define CTE_DE_ENTRADA 1.21825e-3
#define EXCITACAO_PRBS 0
#define EXCITACAO_QUADRADA 1
#define EXCITACAO_SENOIDAL 2
#define EXCITACAO_RAMPA 3

#define PASSO 1
#define JANELA 50
#define N JANELA - 1 
#define ORDEM 2  // ordem do sistema
#define T 5    // período de amostragem ms

void FtF_func(void); // função de atualização da matriz PHI transposta PHI
void FtY_func(void); // função de atualização da matriz PHI transposta Y
void calcular_matriz_inversa(void);
void calcular_theta(void); // calculo dos parâmetros estimados
void atualizar_excitacao(uint16_t k_atual);

ISR(TIMER2_OVF_vect);
ISR(INT0_vect);

void UART_config(void);
void UART_enviaCaractere(unsigned char ch);
void UART_enviaString(char *s);

volatile uint8_t cont1 = 0, cont5 = 0; 
volatile uint16_t cont2 = 0, cont3 = 0, cont4 = 0;
volatile bool print = false;
volatile bool q0 = 1, q1 = 1, q2 = 1, q3 = 1, q_feedback;
volatile bool pode_estimar = false,  estimar = false, print_entrada = false, coletar_dados = true, identificado = false;
volatile double yek = 0, yek_1 = 0, yek_2 = 0; // saída estimada atual e anterior
volatile double uk_1 = 0, uk_2 = 0; // entrada conhecida anterior
volatile uint8_t excitacao = EXCITACAO_PRBS;
volatile double y_sin = 0;
volatile uint16_t y_sin_int = 0, y_rampa = 0;

uint8_t buffer[8];

double u[JANELA]; // vetor dos valores de entradas correspondentes à janela de amostragem
double y[JANELA]; // vetor dos valores de saídas correspondentes à janela de amostragem

double FtF[2*ORDEM][2*ORDEM];   // PHI transposta PHI
double FtY[2*ORDEM][1];   // PHI transposta Y

double inv_FtF[2*ORDEM][2*ORDEM]; // matriz inversa da PHI transposta PHI
double theta[2*ORDEM][1]; // parâmetros estimados

void setup(void)
{
  // TIMER 2 - CONTAGEM
  TCCR2B = (1 << CS22) | (1 << CS21); // prescaler de 256
  TIMSK2 = (1 << TOIE2);              // habilitando a interrupção por overflow
  TCNT2 = 194;                        // para overflow a cada 1ms

  // ADC2
  ADMUX = (1 << REFS0);
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

  // TIMER 1 - MODO FAST PWM
  TCCR1A = (1<<COM1A1) | (1<<COM1B1) | (1<<WGM11); 
  TCCR1B = (1<<WGM13) | (1<<WGM12) | (1<<CS10); // prescale de 1
  ICR1 = 4000; // Para frequência do PWM em 4000 Hz, dado esse prescaler
  
  DDRB = (1<<PB1);
  PORTB &= ~(1<<PB1);

  DDRD = 0x00;
  PORTD = (1<<PD2); // habilitando pull up interno do pino

  EICRA = (1<<ISC01); // seta interrupção por borda de descida do pino PD2
  EIMSK = (1<<INT0); //  habilita interrupção do pino PD2

  // UART
  UART_config();

  // HABILITAR CHAVE GERAL DAS INTERRUPÇÕES
  sei();
}

int main(void)
{
  q_feedback = q0 ^ q1;

  // INICIALIZAÇÃO DOS VETORES
  // ===================================================================
  for (int i = 0; i <= N; i++) {
    y[i] = 0;
    u[i] = 0;
  }

  for (int i = 0; i < 2 * ORDEM; i++) {
    for (int j = 0; j < 2 * ORDEM; j++) {
      FtF[i][j] = 0;
      inv_FtF[i][j] = 0;
    }
  }

  for (int i = 0; i < 2 * ORDEM; i++) {
    FtF[i][0] = 0;
    theta[i][0] = 0;
  }
  // ===================================================================

  setup();

  while (1)
  {

    if (print)
    {       
      // uint8_t ti = TCNT2;

      // VISUALIZAÇÃO DA EXCITAÇÃO DO SISTEMA
      // ===================================================================
      
      sprintf(buffer, "%.3f\t", OCR1A*CTE_DE_ENTRADA);
      UART_enviaString(buffer);

      sprintf(buffer, "%.3f\t", ADC*CTE_DE_SAIDA);
      UART_enviaString(buffer);

      if (identificado) {
        sprintf(buffer, "%.3f\t", yek);
        UART_enviaString(buffer);
      }

      UART_enviaCaractere('\n');
      // ===================================================================

      // TEMPO DO PRINT
      // sprintf(buffer, "%.2f\n", (float) (TCNT2 - ti)*62.5);
      // UART_enviaString(buffer);

      print = false;
    }

    //ESTIMAÇÃO DE PARÂMETROS
    // ===================================================================
    if (pode_estimar) {
      FtF_func();
      FtY_func();            
      
        calcular_matriz_inversa();
        calcular_theta();      

      // for (int i = 0; i < 2 * ORDEM; i++) {
      //   sprintf(buffer, "%.4f\n", theta[i][0]);
      //   UART_enviaString(buffer);
      // }
      // UART_enviaCaractere('\n');

      pode_estimar = false; 
      identificado = true; 
    }
  }

  return 0;
}

// ========================================================================================

ISR(TIMER2_OVF_vect)
{  
  static uint16_t k = 0;
  TCNT2 = 194; // reseta timer 1ms    
  cont1++;
  
  if (cont1 >= T)
  { 
    // TAXA DE AMOSTRAGEM EM MILISSEGUNDOS
    cont1 = 0;
    k++;  
    print = true;

    ADCSRA |= (1<<ADSC); // habilita a leitura do canal do AD
    while (ADCSRA & (1<<ADSC)); // aguarda leitura estar concluida
        
    if (coletar_dados) {
      // GERAÇÃO DE SINAL PRBS DE EXCITAÇÃO PARA ESTIMAÇÃO
      // =============================================================       
      PORTB |= (1<<PB1);

      cont4++;

      if (k % PASSO == 0) {
        q_feedback = q0 ^ q1;
        q0 = q1;
        q1 = q2;
        q2 = q3;
        q3 = q_feedback;
      }

      if (q0 == 1)
        OCR1A = PWM_1_5V;
      else
        OCR1A = PWM_1V;        
      // ===========================================================

      // AQUISIÇÃO DE DADOS
      // ===========================================================
      for (int i = 0; i <= N-1; i++) {
        y[i] = y[i+1];
        u[i] = u[i+1];
      }

      y[N] = (double) ADC*CTE_DE_SAIDA;
      u[N] = (double) OCR1A*CTE_DE_ENTRADA;
    
      if (cont4 >= N+ORDEM-1) {
        pode_estimar = true;
        coletar_dados = false;
      }
    }

    // ESTIMAÇÃO DO SISTEMA IDENTIFICADO
    // ===========================================================
    if (identificado == true) {
      if (cont2 < 400)
        cont2++;    

      if (cont2 < 200) {
        PORTB &= ~(1<<PB0);
        OCR1A = 0;
      }

      if (cont2 >= 400)
        estimar = true;

      if (estimar) {        
        atualizar_excitacao(k);
        yek = - yek_1*theta[0][0] - yek_2*theta[1][0] + uk_1*theta[2][0] + uk_2*theta[3][0];
        yek_2 = yek_1;
        yek_1 = yek;
        uk_2 = uk_1;
        uk_1 = OCR1A*CTE_DE_ENTRADA;      
      }
    }
  }
}


ISR(INT0_vect) {
  excitacao++;

  if (excitacao > EXCITACAO_RAMPA) {
    excitacao = EXCITACAO_PRBS;
  }
}


// FUNÇÃO DE EXCITAÇÃO DURANTE A ESTIMAÇÃO DO SISTEMA
// ==========================================================================
void atualizar_excitacao(uint16_t k_atual) {
  static bool flag_nivel_degrau = 0;  
  static double phase = 0.0;
  static uint16_t i = PWM_1V, sinal_rampa = 1;

  if (identificado == false)
    return;

  switch (excitacao)
  {
  case EXCITACAO_PRBS:
    PORTB |= (1<<PB1);

    if (k_atual % 8 == 0) {
      q_feedback = q0 ^ q1;
      q0 = q1;
      q1 = q2;
      q2 = q3;
      q3 = q_feedback;
    }

    if (q0 == 1)
      OCR1A = PWM_1_5V;
    else
      OCR1A = PWM_1V; 
    break;
  
  case EXCITACAO_QUADRADA:
    PORTB |= (1<<PB1);

    if (k_atual % 200 == 0)
      flag_nivel_degrau = !flag_nivel_degrau;    
      
    if (flag_nivel_degrau == true) 
      OCR1A = PWM_1_5V;  
    else      
      OCR1A = PWM_1V;    
    
    break;
  

  case EXCITACAO_SENOIDAL:
    #define _2_PI_f_k 0.157079633
    
    y_sin = sin(phase);
    OCR1A = (uint16_t) (1020 + y_sin*220);
    phase += _2_PI_f_k;
    break;


  case EXCITACAO_RAMPA:
    if (k_atual % 1 == 0) {
      if (i == PWM_1V)
        sinal_rampa = 1;
      if (i == PWM_4V)
        sinal_rampa = -1;      
      
      i += sinal_rampa;
      OCR1A = i;
    }
    
    break;


  default:
    excitacao = EXCITACAO_PRBS;
    break;
  }
}
// ==========================================================================


// FUNÇÕES DE CALCULOS
// ==========================================================================
void FtF_func(void)
{
  double  a00 = 0, a01 = 0, a02 = 0, a03 = 0,
          a10 = 0, a11 = 0, a12 = 0, a13 = 0,
          a20 = 0, a21 = 0, a22 = 0, a23 = 0,
          a30 = 0, a31 = 0, a32 = 0, a33 = 0;

	for (int i = 0; i < N-1; i++) {
		a00 += y[i+1]*y[i+1];
		a01 += y[i+1]*y[i];
		a02 += y[i+1]*u[i+1]*(-1);
		a03 += y[i+1]*u[i]*(-1);

    a10 += y[i]*y[i+1];
		a11 += y[i]*y[i];
		a12 += y[i]*u[i+1]*(-1);
		a13 += y[i]*u[i]*(-1);

    a20 += u[i+1]*y[i+1]*(-1);
		a21 += u[i+1]*y[i]*(-1);
		a22 += u[i+1]*u[i+1];
		a23 += u[i+1]*u[i];

    a30 += u[i]*y[i+1]*(-1);
		a31 += u[i]*y[i]*(-1);
		a32 += u[i]*u[i+1];
		a33 += u[i]*u[i];
	}

	FtF[0][0] = a00;
  FtF[0][1] = a01;
  FtF[0][2] = a02;
	FtF[0][3] = a03;

  FtF[1][0] = a10;
  FtF[1][1] = a11;
  FtF[1][2] = a12;
  FtF[1][3] = a13;

  FtF[2][0] = a20;
  FtF[2][1] = a21;
  FtF[2][2] = a22;
  FtF[2][3] = a23;

  FtF[3][0] = a30;
  FtF[3][1] = a31;
  FtF[3][2] = a32;
  FtF[3][3] = a33;
}

void FtY_func(void) {
  double  b00 = 0, 
          b10 = 0,
          b20 = 0,  
          b30 = 0;

  for (int i = 0; i < N-1; i++) {
    b00 += y[i+2]*y[i+1]*(-1);
    b10 += y[i+2]*y[i]*(-1);
    b20 += y[i+2]*u[i+1];
    b30 += y[i+2]*u[i];
	}

  FtY[0][0] = b00;
  FtY[1][0] = b10;
  FtY[2][0] = b20;
  FtY[3][0] = b30;
}


void calcular_matriz_inversa(void)
{
  const int SIZE = 4;
  double matrizAumentada[SIZE][2 * SIZE];
  for (int i = 0; i < SIZE; i++)
  {
    for (int j = 0; j < SIZE; j++)
    {
      matrizAumentada[i][j] = FtF[i][j];
      // Definir elementos diagonais da matriz identidade
      if (i == j)
      {
        matrizAumentada[i][j + SIZE] = 1.0;
      }
      else
      {
        matrizAumentada[i][j + SIZE] = 0.0;
      }
    }
  }

  // Realizar a eliminação de Gauss-Jordan
  for (int i = 0; i < SIZE; i++)
  {
    // Pivô atual
    double pivot = matrizAumentada[i][i];

    // Dividir a linha pelo pivô
    for (int j = 0; j < 2 * SIZE; j++)
    {
      matrizAumentada[i][j] /= pivot;
    }

    // Subtrair múltiplos da linha atual das outras linhas
    for (int k = 0; k < SIZE; k++)
    {
      if (k != i)
      {
        double factor = matrizAumentada[k][i];
        for (int j = 0; j < 2 * SIZE; j++)
        {
          matrizAumentada[k][j] -= factor * matrizAumentada[i][j];
        }
      }
    }
  }

  // Extrair a matriz inversa da matriz aumentada
  for (int i = 0; i < SIZE; i++)
  {
    for (int j = 0; j < SIZE; j++)
    {
      inv_FtF[i][j] = matrizAumentada[i][j + SIZE];
    }
  }
}


void calcular_theta(void)
{
  for (int i = 0; i < 2*ORDEM; i++) {
    for (int j = 0; j < 1; j++) {
      theta[i][j] = 0;
    }
  }

  for (int i = 0; i < 2*ORDEM; i++)
  {
    for (int j = 0; j < 1; j++)
    {
      for (int k = 0; k < 2*ORDEM; k++)
      {
        theta[i][j] += inv_FtF[i][k] * FtY[k][j];
      }      
    }    
  }
}
// ==========================================================================


// UART
// ==========================================================================
void UART_config(void)
{
  // Baud Rate de 115.2 kbps para um cristal de 16MHz (Datasheet)
  UBRR0 = 8;

  // Habilita a interrupção de recepção e os pinos TX e RX
  UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);

  // Configura a UART com 8 bits de dados
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void UART_enviaCaractere(unsigned char ch)
{
  // Aguarda o buffer ser desocupado
  while (!(UCSR0A & (1 << UDRE0)));

  UDR0 = ch;
}

void UART_enviaString(char *s)
{
  unsigned int i = 0;
  while (s[i] != '\x0')
  {
    UART_enviaCaractere(s[i++]);
  };
}
