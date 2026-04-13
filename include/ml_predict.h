#ifndef ML_PREDICT_H
#define ML_PREDICT_H

// ============================================================
// TinyML Inference — Decision Tree exported as if-else
// ============================================================

// Returns allocation mode:
//   0 = SEQUENTIAL
//   1 = RANDOM
//   2 = WEAR_AWARE
int ml_predict(float avg_write_size, float writes_per_sec);

#endif // ML_PREDICT_H
