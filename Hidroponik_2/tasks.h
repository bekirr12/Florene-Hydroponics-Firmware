// ============================================================
//  TASKS.H - FreeRTOS gorev orkestrasyonu
// ============================================================

#ifndef HIDRO_TASKS_H
#define HIDRO_TASKS_H

// ControlTask (core 1) + NetworkTask (core 0) olusturup baslatir.
// setup() icinde, tum donanim/durum init'i tamamlandiktan sonra cagrilir.
void tasksStart();

#endif // HIDRO_TASKS_H
