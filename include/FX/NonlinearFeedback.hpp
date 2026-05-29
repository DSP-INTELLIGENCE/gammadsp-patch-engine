struct NonlinearFeedback
{
    float amount = 1.0f;

    inline float operator()(float x)
    {
        return std::tanh(x * amount);
    }
};
