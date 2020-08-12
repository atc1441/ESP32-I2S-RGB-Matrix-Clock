/*
 * This code is a modified version of this Library:
 * 
 * https://github.com/mrfaptastic/ESP32-RGB64x32MatrixPanel-I2S-DMA
 * 
 * 
 * License is MIT
 * https://github.com/mrfaptastic/ESP32-RGB64x32MatrixPanel-I2S-DMA/blob/master/LICENSE.txt
 */

#pragma once

int MaxLed = 256;

#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 15
#define P_OE 2
#define CLK 14
#define R1 13
#define R2 27
#define G1 21
#define G2 17
#define B1 12
#define B2 4

void set_RGB(bool state) {
  digitalWrite (R1, state);
  digitalWrite (G1, state);
  digitalWrite (B1, state);
  digitalWrite (R2, state);
  digitalWrite (G2, state);
  digitalWrite (B2, state);
}
void init_matrix() {
  bool C12[16] = {1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  bool C13[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};

  pinMode(CLK, OUTPUT);
  pinMode(P_LAT, OUTPUT);
  pinMode(P_OE, OUTPUT);
  pinMode(B1, OUTPUT);
  pinMode(R1, OUTPUT);
  pinMode(R2, OUTPUT);
  pinMode(G1, OUTPUT);
  pinMode(G2, OUTPUT);
  pinMode(B2, OUTPUT);
  pinMode(P_A, OUTPUT);
  pinMode(P_B, OUTPUT);
  pinMode(P_C, OUTPUT);
  pinMode(P_D, OUTPUT);
  pinMode(P_E, OUTPUT);

  digitalWrite (P_OE, HIGH);
  digitalWrite (P_LAT, LOW);
  digitalWrite (CLK, LOW);

  // Send Data to control register 11
  for (int l = 0; l < MaxLed; l++) {
    int y = l % 16;
    set_RGB(LOW);
    if (C12[y]) set_RGB(HIGH);
    digitalWrite(P_LAT, (l > MaxLed - 12) ? HIGH : LOW);
    digitalWrite(CLK, HIGH);
    digitalWrite(CLK, LOW);
  }
  digitalWrite (P_LAT, LOW);
  digitalWrite (CLK, LOW);

  // Send Data to control register 12
  for (int l = 0; l < MaxLed; l++) {
    int y = l % 16;
    set_RGB(LOW);
    if (C13[y])set_RGB(HIGH);
    digitalWrite(P_LAT, (l > MaxLed - 13) ? HIGH : LOW);
    digitalWrite(CLK, HIGH);
    digitalWrite(CLK, LOW);
  }
  digitalWrite (P_LAT, LOW);
  digitalWrite (CLK, LOW);
}
