// Auto-generated from trained Decision Tree
// Paste this into ml_predict.cpp if you retrain

int ml_predict(float avg_write_size, float writes_per_sec)
{
    if (avg_write_size <= 511.4f)
    {
        if (writes_per_sec <= 30.0f)
        {
            return 2; // WEAR_AWARE
        }
        else
        {
            return 1; // RANDOM
        }
    }
    else
    {
        if (writes_per_sec <= 10.0f)
        {
            if (avg_write_size <= 1023.1f)
            {
                return 2; // WEAR_AWARE
            }
            else
            {
                return 0; // SEQUENTIAL
            }
        }
        else
        {
            if (avg_write_size <= 511.9f)
            {
                return 2; // WEAR_AWARE
            }
            else
            {
                return 2; // WEAR_AWARE
            }
        }
    }
}
