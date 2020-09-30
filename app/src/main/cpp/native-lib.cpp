#include <jni.h>
#include <string>
#include <unistd.h>
#include <sys/ptrace.h>
#include <android/log.h>
#include <wait.h>
#include <errno.h>
#define LOG_TAG "BT"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__) // 定义LOGD类型
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__) // 定义LOGI类型
#define LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__) // 定义LOGW类型
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__) // 定义LOGE类型
#define LOGF(...)  __android_log_print(ANDROID_LOG_FATAL,LOG_TAG,__VA_ARGS__) // 定义LOGF类

extern "C" JNIEXPORT jstring JNICALL
Java_com_pareto_selfptrace_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    pid_t pid  = getpid();
    LOGD("begin test , i'm %d\n",pid);
    pid_t childid = fork();
    if(childid == -1){
        LOGD("fork error \n");
    }
    //子进程附加父进程
    if(childid == 0 ){
        int err= 0;

        do{
            err = ptrace(PTRACE_ATTACH , pid , (void*)0 , (void*)0 );
            LOGD("keep ptracing\n");
        }
        while ( err == -1 && (errno == EBUSY || errno == EPERM));
        if(err == -1){
            LOGD("child process attach failed\n");
        }
        else{
            int status;
            err = waitpid(pid , &status , __WALL );
            if(err == -1  || err != pid){
                LOGD("let me see what happend %d\n", errno);
            }
            else{
                unsigned int dispose_signal = 0;
                //线程处于停止状态。
                if (WIFSTOPPED(status)) {
                    //PTRACE_O_TRACEFORK 被跟踪进程在下次调用fork()时停止执行并自动跟踪新产生的进程，新产生的进程刚开始收到SIGSTOP信号。其新产生的进程的pid可以通过       PTRACE_GETEVENTMSG 得到
                    //PTRACE_O_TRACEVFORK 被跟踪进程在下次调用vfork()时停止执行，并自动跟踪新产生的进程，新产生的进程刚开始收到SIGSTOP信号。其新产生的进程的pid可以通过   PTRACE_GETEVENTMSG 得到
                    //PTRACE_O_TRACECLONE 被跟踪进程在下一次调用clone()时将其停止，并自动跟踪新产生的进程，新产生的进程刚开始收到SIGSTOP信号。其新产生的进程的pid可以通过 PTRACE_GETEVENTMSG 得到
                    //PTRACE_O_TRACEEXEC:被跟踪进程在下一次调用exec()函数时使其停止。
                    ptrace(PT_SETOPTIONS, pid, 0,
                               (void *) (PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
                                         PTRACE_O_TRACECLONE |
                                         PTRACE_O_TRACEEXEC));
                    //返回导致进程停止的信号。
                    dispose_signal = WSTOPSIG(status);
                    LOGD("I'm stopped by %\n",dispose_signal);

//                    INIT_DEBUG_LOG("[SelfPtrace::TraceAllThreads] child stop %d, signal:%d", p,
//                          dispose_signal);

                    if (dispose_signal == SIGSTOP) {
                        dispose_signal = 0;
                    }
                }
                    //线程正常退出，获取线程退出时状态。
                else if (WIFEXITED(status)) {
                    dispose_signal = WEXITSTATUS(status);
                }
                    // 进程异常终止状态，获取终止的信号。
                else if (WIFSIGNALED(status)) {
                    dispose_signal = WTERMSIG(status);
                }
                //线程继续执行，并发送dispose_signal信号。作用还原进程状态。
                ptrace(PTRACE_CONT, pid, 0, (void *) dispose_signal);
                while(1){
                    int status = 0;
                    unsigned int dispose_signal = 0;
                    pid_t p = waitpid(-1, &status, __WALL | WUNTRACED);
                    // 进程停止 ，终止 或者退出
                    if (WIFSTOPPED(status)) {
                        dispose_signal = WSTOPSIG(status);
                        if (dispose_signal == SIGTRAP || dispose_signal == SIGSTOP) {
                            dispose_signal = 0;
                        }
                    } else if (WIFSIGNALED(status)) {
                        dispose_signal = WTERMSIG(status);
                    } else if (WIFEXITED(status)) {
                        dispose_signal = WEXITSTATUS(status);
                    }
                    int err = ptrace(PTRACE_CONT, p, 0, (void *) dispose_signal);
                    //只要不是找不到线程则打印。
                    if (err == -1 && errno != ESRCH) {
                        LOGD("maybe proces  exited\n");
                    }
                }
            }
        }
    }
    else if(childid > 0 ){
        //父进程继续运行
        while(1){
            LOGD("i wanna tell you i'm running\n");
            sleep(3);
        }
    }
    return env->NewStringUTF(hello.c_str());
}
