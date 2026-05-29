#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <XPT2046_Touchscreen.h>
#include <qrcode_st7789.h>


String pixKey = "chave pix";
String nomePix = "nome";
String cidadePix = "cidade";


// ================= TFT =================
#define TFT_CS    10
#define TFT_DC    2
#define TFT_RST   3
#define TFT_MOSI  5
#define TFT_SCLK  7

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
QRcode_ST7789 qrcode(&tft);

// ================= TOUCH =================
#define TOUCH_CS   8
#define TOUCH_IRQ  9

XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// ================= EXPRESSÃO =================
String expr = "";

// ================= BOTÕES =================
String botoes[5][4] = {
  {"7","8","9","/"},
  {"4","5","6","*"},
  {"1","2","3","+"},
  {".","0","=","-"},
  {"PIX","DEL","C",""}
};


String crc16(String payload) {

  uint16_t polinomio = 0x1021;
  uint16_t resultado = 0xFFFF;

  for (int i = 0; i < payload.length(); i++) {

    resultado ^= (payload[i] << 8);

    for (int bit = 0; bit < 8; bit++) {

      if (resultado & 0x8000)
        resultado = (resultado << 1) ^ polinomio;
      else
        resultado <<= 1;
    }
  }

  resultado &= 0xFFFF;

  char crc[5];

  sprintf(crc, "%04X", resultado);

  return String(crc);
}


String gerarPayloadPix(String valor) {

  String txid = "ESPPIX";

  String merchantAccount =
    "0014BR.GOV.BCB.PIX" +
    String("01") +
    (pixKey.length() < 10 ? "0" : "") +
    String(pixKey.length()) +
    pixKey;

  String payload =
    "000201"

    // Merchant Account
    + String("26")
    + (merchantAccount.length() < 10 ? "0" : "")
    + String(merchantAccount.length())
    + merchantAccount

    // MCC
    + "52040000"

    // Moeda BRL
    + "5303986"

    // Valor
    + "54"
    + (valor.length() < 10 ? "0" : "")
    + String(valor.length())
    + valor

    // País
    + "5802BR"

    // Nome
    + "59"
    + (nomePix.length() < 10 ? "0" : "")
    + String(nomePix.length())
    + nomePix

    // Cidade
    + "60"
    + (cidadePix.length() < 10 ? "0" : "")
    + String(cidadePix.length())
    + cidadePix

    // TXID
    + "62"
    + String(
        ((String("05") +
        (txid.length() < 10 ? "0" : "") +
        String(txid.length()) +
        txid).length()) < 10 ? "0" : ""
      )
    + String(
        (String("05") +
        (txid.length() < 10 ? "0" : "") +
        String(txid.length()) +
        txid).length()
      )

    + "05"
    + (txid.length() < 10 ? "0" : "")
    + String(txid.length())
    + txid

    // CRC
    + "6304";

  payload += crc16(payload);
  Serial.println(payload);
  return payload;
}


// ================= DESENHAR =================
void desenhar() {

  tft.fillScreen(0x8410);

  // visor
  tft.fillRect(5, 5, 310, 50, ST77XX_GREEN);

  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(2);

  tft.setCursor(10, 22);
  tft.print(expr);

  // botões
  for(int y=0;y<5;y++) {

    for(int x=0;x<4;x++) {

      String txt = botoes[y][x];
      
      if(txt == "") continue;

      int bx = 10 + (x * 75);
      int by = 70 + (y * 33);

      uint16_t cor = ST77XX_WHITE;

      if(txt == "=") cor = ST77XX_GREEN;
      if(txt == "C") cor = ST77XX_MAGENTA;
      if(txt == "DEL") cor = ST77XX_BLUE;
      if(txt == "PIX") cor = 0xFFE0;

      tft.fillRoundRect(bx, by, 65, 28, 5, cor);

      tft.setTextColor(ST77XX_BLACK);
      tft.setTextSize(2);

      int cx = bx + 10;

      if(txt.length() == 1)
        cx = bx + 25;

      tft.setCursor(cx, by + 8);
      tft.print(txt);
    }
  }
}

// ================= PRIORIDADE =================
bool operador(char c) {
  return c=='+' || c=='-' || c=='*' || c=='/';
}

int prioridade(char op) {

  if(op=='+' || op=='-') return 1;
  if(op=='*' || op=='/') return 2;

  return 0;
}

float aplicar(float a, float b, char op) {

  if(op=='+') return a+b;
  if(op=='-') return a-b;
  if(op=='*') return a*b;
  if(op=='/') return a/b;

  return 0;
}

// ================= CALCULAR =================
float calcularExpressao(String s) {

  float valores[50];
  char ops[50];

  int vTop = -1;
  int oTop = -1;

  int i = 0;

  while(i < s.length()) {

    if(s[i]==' ') {
      i++;
      continue;
    }

    if(s[i]=='-' && (i==0 || operador(s[i-1]) || s[i-1]=='(')) {

      String num = "-";
      i++;

      while(i < s.length() &&
           ((s[i]>='0' && s[i]<='9') || s[i]=='.')) {

        num += s[i];
        i++;
      }

      valores[++vTop] = num.toFloat();
      continue;
    }

    if((s[i]>='0' && s[i]<='9') || s[i]=='.') {

      String num = "";

      while(i < s.length() &&
           ((s[i]>='0' && s[i]<='9') || s[i]=='.')) {

        num += s[i];
        i++;
      }

      valores[++vTop] = num.toFloat();
      continue;
    }

    if(operador(s[i])) {

      while(oTop>=0 &&
            prioridade(ops[oTop]) >= prioridade(s[i])) {

        float b = valores[vTop--];
        float a = valores[vTop--];

        char op = ops[oTop--];

        valores[++vTop] = aplicar(a,b,op);
      }

      ops[++oTop] = s[i];
    }

    i++;
  }

  while(oTop>=0) {

    float b = valores[vTop--];
    float a = valores[vTop--];

    char op = ops[oTop--];

    valores[++vTop] = aplicar(a,b,op);
  }

  return valores[vTop];
}

// ================= TOUCH =================
void touch() {

  if(ts.touched()) {

    TS_Point p = ts.getPoint();

    int tx = map(p.x, 200, 3800, 0, 320);
    int ty = map(p.y, 200, 3800, 0, 240);

    tx = constrain(tx, 0, 319);
    ty = constrain(ty, 0, 239);

    for(int y=0;y<5;y++) {

      for(int x=0;x<4;x++) {

        String b = botoes[y][x];

        int bx = 10 + (x * 75);
        int by = 70 + (y * 33);

        if(tx > bx && tx < bx+65 &&
           ty > by && ty < by+28) {

          if(b == "C") {
            expr = "";
          }

          else if(b == "DEL") {
            if(expr.length() > 0)
              expr.remove(expr.length()-1);
          }

          else if(b == "=") {
            float r = calcularExpressao(expr);
            expr = String(r);
          }

          // BOTÃO PIX (SÓ VISUAL POR ENQUANTO)
          else if(b == "PIX") {
              float r = calcularExpressao(expr);

                if(r <= 0) return;

                char buffer[16];
                sprintf(buffer, "%.2f", r);

                String valorPix = String(buffer);

                String payload = gerarPayloadPix(valorPix);

                tft.fillScreen(ST77XX_BLACK);

                tft.setRotation(0);

                qrcode.init();

                delay(100);

                qrcode.create(payload.c_str());

                while(true) {

                  if(ts.touched()) {

                    delay(300);

                    tft.setRotation(1);

                    desenhar();

                    return;
              }
            }
          }

          else {
            expr += b;
          }

          desenhar();
          delay(180);
        }
      }
    }
  }
}

// ================= SETUP =================
void setup() {

  SPI.begin(TFT_SCLK, 0, TFT_MOSI);
  
  qrcode.init();
  
  tft.init(240, 320);
  tft.setRotation(1);
  tft.invertDisplay(false);

  ts.begin();
  ts.setRotation(1);


  desenhar();
}

// ================= LOOP =================
void loop() {
  touch();
}