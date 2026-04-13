#include "ml_predict.h"

// ============================================================
// TinyML Inference — Decision Tree exported as if-else
// Trained on synthetic workload dataset
//
// Features:
//   avg_write_size (bytes, 64-4096)
//   writes_per_sec (1-100)
//
// Output:
//   0 = SEQUENTIAL (large files, low throughput)
//   1 = RANDOM     (small files, high throughput)
//   2 = WEAR_AWARE (default / balanced)
// ============================================================

int ml_predict(float avg_write_size, float writes_per_sec)
{
    if (avg_write_size <= 511.4f)
    {
        if (writes_per_sec <= 30.0f)
            return 2; // WEAR_AWARE
        else
            return 1; // RANDOM
    }
    else
    {
        if (writes_per_sec <= 10.0f)
        {
            if (avg_write_size <= 1023.1f)
                return 2; // WEAR_AWARE
            else
                return 0; // SEQUENTIAL
        }
        else
        {
            return 2; // WEAR_AWARE
        }
    }
}
