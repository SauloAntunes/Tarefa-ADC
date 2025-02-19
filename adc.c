// Chamada das bibliotecas
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "lib/ssd1306.h"
#include "lib/font.h"


// Definições dos pinos e dos demais valores que serão utilizados no programa
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C 
#define JOYSTICK_X_PIN 26
#define JOYSTICK_Y_PIN 27
#define JOYSTICK_PB 22
#define BUTTON_A 5
#define BUTTON_B 6
#define LED_G 11
#define LED_B 12
#define LED_R 13
#define SCREEN_WIDTH 125
#define SCREEN_HEIGHT 62


// Variáveis globais para controle do tempo do último evento, inicialização do display e controle dos LEDs com PWM
static volatile uint32_t last_time = 0;
ssd1306_t ssd;
bool enable_disable_pwm_led = true;


// Prototipação das rotinas
void setup();
uint pwm_init_gpio(uint gpio, uint wrap);
void gpio_irq_handler(uint gpio, uint32_t events);


// Rotina principal
int main()
{
  // Inicialização e configuração dos pinos e periféricos
  setup();

  // Configuração da interrupção com callback para os botões A, B e do joystick
  gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  gpio_set_irq_enabled_with_callback(JOYSTICK_PB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  
  // Variáveis locais para configuração e controle
  uint16_t adc_value_x;
  uint16_t adc_value_y;  
  uint pwm_wrap = 4096;  
  uint pwm_slice = pwm_init_gpio(LED_B, pwm_wrap);
  pwm_slice = pwm_init_gpio(LED_R, pwm_wrap);
  uint led_level_b = 0;
  uint led_level_r = 0;
  bool cor = true;
  
  while (1)
  {
    adc_select_input(0); // Seleciona o ADC para eixo X. O pino 26 como entrada analógica
    adc_value_x = adc_read();
    adc_select_input(1); // Seleciona o ADC para eixo Y. O pino 27 como entrada analógica
    adc_value_y = adc_read();

    if (enable_disable_pwm_led)
    {
      // Ajustar o brilho do LED vermelho com base no valor do eixo X
      if (adc_value_x < 1975)
      {
        led_level_r = (1975 - adc_value_x) * 2;
      }
      else if (adc_value_x > 2020)
      {
        led_level_r = (adc_value_x - 2020) * 2;
      }
      else
      {
        led_level_r = 0;
      }

      // Ajustar o brilho do LED azul com base no valor do eixo Y
      if (adc_value_y < 2150)
      {
        led_level_b = (2150 - adc_value_y) * 2;
      }
      else if (adc_value_y > 2200)
      {
        led_level_b = (adc_value_y - 2200) * 2;
      }
      else
      {
        led_level_b = 0;
      }

      pwm_set_gpio_level(LED_B, led_level_b);
      pwm_set_gpio_level(LED_R, led_level_r);
    }
    else
    {
      pwm_set_gpio_level(LED_B, 0);
      pwm_set_gpio_level(LED_R, 0);
    }

    // Mapear os valores lidos dos ADCs para a área do display
    int x = (adc_value_x * (SCREEN_WIDTH - 8)) / 4095;
    int y = (adc_value_y * (SCREEN_HEIGHT - 8)) / 4095;

    // Limitar a posição do quadrado para não ultrapassar as bordas do display
    if (x < 0) x = 0;
    if (x > SCREEN_WIDTH - 8) x = SCREEN_WIDTH - 8;
    if (y < 0) y = 0;
    if (y > SCREEN_HEIGHT - 8) y = SCREEN_HEIGHT - 8;

    // Atualiza o conteúdo do display com animações
    ssd1306_fill(&ssd, !cor); // Limpa o display
    ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo
    
    // Desenha o quadrado na posição atualizada
    ssd1306_rect(&ssd, y, x, 8, 8, cor, true); // Desenha o quadrado
    
    // Desenha um retângulo adicional ao pressionar o botão do joystick
    if (!gpio_get(JOYSTICK_PB))
    {
        ssd1306_rect(&ssd, 4, 4, 120, 58, cor, !cor);
    }

    ssd1306_send_data(&ssd); // Atualiza o display

    sleep_ms(100);
  }

}

// Inicializa e configura os pinos e periféricos que serão utilizados no programa
void setup()
{
  // Inicializa o pino do LED verde, define a direção como saída
  gpio_init(LED_G);
  gpio_set_dir(LED_G, GPIO_OUT);

  // Inicializa o pino do LED vermelho, define a direção como saída
  gpio_init(LED_R);
  gpio_set_dir(LED_R, GPIO_OUT);

   // Inicializa o pino do botão A e define a direção como entrada, com pull-up
  gpio_init(BUTTON_A);
  gpio_set_dir(BUTTON_A, GPIO_IN);
  gpio_pull_up(BUTTON_A);
  
   // Inicializa o pino do botão B e define a direção como entrada, com pull-up
  gpio_init(BUTTON_B);
  gpio_set_dir(BUTTON_B, GPIO_IN);
  gpio_pull_up(BUTTON_B);

   // Inicializa o pino do botão do joystick e define a direção como entrada, com pull-up
  gpio_init(JOYSTICK_PB);
  gpio_set_dir(JOYSTICK_PB, GPIO_IN);
  gpio_pull_up(JOYSTICK_PB); 

  // I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_pull_up(I2C_SDA); // Pull up the data line
  gpio_pull_up(I2C_SCL); // Pull up the clock line
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT); // Inicializa o display
  ssd1306_config(&ssd); // Configura o display
  ssd1306_send_data(&ssd); // Envia os dados para o display

  // Limpa o display. O display inicia com todos os pixels apagados.
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

  adc_init();
  adc_gpio_init(JOYSTICK_X_PIN);
  adc_gpio_init(JOYSTICK_Y_PIN);  
}

// Função para inicializar o PWM em um pino GPIO
uint pwm_init_gpio(uint gpio, uint wrap)
{
  gpio_set_function(gpio, GPIO_FUNC_PWM);
  uint slice_num = pwm_gpio_to_slice_num(gpio);
  pwm_set_wrap(slice_num, wrap);
  pwm_set_enabled(slice_num, true);

  return slice_num;  
}

// Função de interrupção com debouncing
void gpio_irq_handler(uint gpio, uint32_t events)
{
  // Obtém o tempo atual em microssegundos
  uint32_t current_time = to_us_since_boot(get_absolute_time());
    
  // Verifica se passou tempo suficiente desde o último evento
  if (current_time - last_time > 200000) // 200 ms de debouncing
  {
    last_time = current_time; // Atualiza o tempo do último evento

    // Alterna o estado dos LEDs PWM
    if (gpio == 5)
    {
      enable_disable_pwm_led = !enable_disable_pwm_led;
    }
    // Modo BOOTSEL com botão B
    else if (gpio == 6)
    {
      reset_usb_boot(0, 0);
    }
    // Alterna o estado do LED verde
    else if (gpio == 22)
    {
      gpio_put(LED_G, !gpio_get(LED_G));
    }
  }
}