#ifndef SINGLE_INSTANCE_H
#define SINGLE_INSTANCE_H

class SingleInstance
{
public:
    enum class Result { Acquired, AlreadyRunning, Error };

    SingleInstance() = default;
    ~SingleInstance();
    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    Result acquire();
    static void markMainWindow(void* window);
    static void unmarkMainWindow(void* window);

private:
#ifdef _WIN32
    void* m_mutex = nullptr;
#endif
};

#endif // SINGLE_INSTANCE_H
