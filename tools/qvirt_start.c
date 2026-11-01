// Start a VM through the stock vendor.qti.qvirt IVirtualizationService (AIDL, raw NDK binder).
#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef AIBinder *(*getsvc_t)(const char *);
static void *on_create(void *a) { return a; }
static void on_destroy(void *u) { (void)u; }
static binder_status_t on_tx(AIBinder *b, transaction_code_t c, const AParcel *i, AParcel *o) {
    (void)b; (void)c; (void)i; (void)o; return STATUS_UNKNOWN_TRANSACTION;
}

static int call(AIBinder *b, int code, const char *str, AIBinder **out_binder, int32_t *out_int) {
    AParcel *in = NULL, *out = NULL;
    binder_status_t st = AIBinder_prepareTransaction(b, &in);
    if (st) { printf("prepare %d\n", st); return st; }
    if (str) AParcel_writeString(in, str, (int32_t)strlen(str));
    st = AIBinder_transact(b, code, &in, &out, 0);
    if (st) { printf("transact code %d -> %d\n", code, st); return st; }
    AStatus *as = NULL;
    st = AParcel_readStatusHeader(out, &as);
    if (st) { printf("status hdr %d\n", st); return st; }
    if (!AStatus_isOk(as)) {
        printf("code %d: exception %d service-specific %d msg=%s\n", code,
               AStatus_getExceptionCode(as), AStatus_getServiceSpecificError(as),
               AStatus_getMessage(as));
        return -1;
    }
    if (out_binder) AParcel_readStrongBinder(out, out_binder);
    if (out_int) AParcel_readInt32(out, out_int);
    AParcel_delete(out);
    return 0;
}

int main(int argc, char **argv) {
    const char *vm = argc > 1 ? argv[1] : "trustedvm";
    int do_start = argc > 2 && !strcmp(argv[2], "start");
    void *h = dlopen("libbinder_ndk.so", RTLD_NOW);
    getsvc_t get = (getsvc_t)dlsym(h, "AServiceManager_checkService");
    if (!get) { printf("no checkService\n"); return 1; }
    AIBinder *svc = get("vendor.qti.qvirt.IVirtualizationService/default");
    if (!svc) { printf("service not found\n"); return 1; }
    AIBinder_Class *cs = AIBinder_Class_define("vendor.qti.qvirt.IVirtualizationService", on_create, on_destroy, on_tx);
    AIBinder_Class *cv = AIBinder_Class_define("vendor.qti.qvirt.IVirtualMachine", on_create, on_destroy, on_tx);
    printf("associate svc %d\n", AIBinder_associateClass(svc, cs));
    AIBinder *v = NULL;
    if (call(svc, 1, vm, &v, NULL) || !v) { printf("getVm failed\n"); return 1; }
    printf("associate vm %d\n", AIBinder_associateClass(v, cv));
    int32_t state = -99;
    call(v, 1, NULL, NULL, &state);
    printf("%s state %d\n", vm, state);
    if (do_start) {
        int r = call(v, 3, NULL, NULL, NULL);
        printf("start -> %d\n", r);
        for (int i = 0; i < 10; i++) {
            sleep(1); state = -99; call(v, 1, NULL, NULL, &state);
            printf("t=%ds state %d\n", i + 1, state); fflush(stdout);
        }
    }
    return 0;
}
