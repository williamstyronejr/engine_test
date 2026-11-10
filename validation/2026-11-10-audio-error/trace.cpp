#include <alsa/asoundlib.h>
#include <dlfcn.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>

template <class T> T original(const char* name) {
    auto function = reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
    if (!function)
        std::abort();
    return function;
}
void record(const char* operation, snd_pcm_t* pcm, long result, unsigned long requested = 0) {
    timespec time{};
    clock_gettime(CLOCK_MONOTONIC, &time);
    const int state = pcm ? static_cast<int>(snd_pcm_state(pcm)) : -1;
    std::fprintf(stderr,
                 "[alsa trace] mono=%lld.%09ld operation=%s result=%ld requested=%lu "
                 "state=%d state_name=%s type=%s\n",
                 static_cast<long long>(time.tv_sec), time.tv_nsec, operation, result, requested,
                 state, pcm ? snd_pcm_state_name(static_cast<snd_pcm_state_t>(state)) : "none",
                 pcm ? snd_pcm_type_name(snd_pcm_type(pcm)) : "none");
}
bool report_error(long result) {
    return result < 0 && result != -EAGAIN && result != -EINTR;
}
extern "C" int snd_pcm_open(snd_pcm_t** pcm, const char* name, snd_pcm_stream_t stream, int mode) {
    static auto call = original<decltype(&snd_pcm_open)>("snd_pcm_open");
    const int result = call(pcm, name, stream, mode);
    record("open", result >= 0 ? *pcm : nullptr, result);
    return result;
}
extern "C" snd_pcm_sframes_t snd_pcm_writei(snd_pcm_t* pcm, const void* data,
                                            snd_pcm_uframes_t frames) {
    static auto call = original<decltype(&snd_pcm_writei)>("snd_pcm_writei");
    const auto result = call(pcm, data, frames);
    if (report_error(result) || (result > 0 && static_cast<snd_pcm_uframes_t>(result) > frames))
        record("write", pcm, result, frames);
    return result;
}
extern "C" int snd_pcm_wait(snd_pcm_t* pcm, int timeout) {
    static auto call = original<decltype(&snd_pcm_wait)>("snd_pcm_wait");
    const int result = call(pcm, timeout);
    if (report_error(result))
        record("wait", pcm, result);
    return result;
}
extern "C" int snd_pcm_prepare(snd_pcm_t* pcm) {
    static auto call = original<decltype(&snd_pcm_prepare)>("snd_pcm_prepare");
    const int result = call(pcm);
    if (report_error(result))
        record("prepare", pcm, result);
    return result;
}
extern "C" int snd_pcm_resume(snd_pcm_t* pcm) {
    static auto call = original<decltype(&snd_pcm_resume)>("snd_pcm_resume");
    const int result = call(pcm);
    if (report_error(result))
        record("resume", pcm, result);
    return result;
}
