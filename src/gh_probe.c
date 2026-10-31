#define SYS_openat 56
#define SYS_ioctl  29
#define SYS_write  64
#define SYS_exit   93
#define SYS_nanosleep 101
#define AT_FDCWD  -100
#define O_RDWR    2
static long sc(long n,long a,long b,long c,long d){register long x8 __asm__("x8")=n;register long x0 __asm__("x0")=a;register long x1 __asm__("x1")=b;register long x2 __asm__("x2")=c;register long x3 __asm__("x3")=d;__asm__ volatile("svc #0":"+r"(x0):"r"(x8),"r"(x1),"r"(x2),"r"(x3):"memory");return x0;}
static void w(const char*s,long n){sc(SYS_write,1,(long)s,n,0);}
static void hx(long v){char b[20];const char*d="0123456789abcdef";b[0]='0';b[1]='x';for(int i=0;i<16;i++)b[2+i]=d[(v>>((15-i)*4))&15];b[18]='\n';w(b,19);}
static void sleep_ms(long ms){ struct { long s; long ns; } t = { ms/1000, (ms%1000)*1000000 }; sc(SYS_nanosleep, (long)&t, 0, 0, 0); }
static const char dev[]="/dev/qgunyah";
static char nm[16]; static char back[16]; static unsigned char run[4096];
void _start(void)
{
    const char *s="trustedvm"; int i=0; while(s[i]){nm[i]=s[i];i++;}
    long fd=sc(SYS_openat,AT_FDCWD,(long)dev,O_RDWR,0); w("open: ",6); hx(fd);
    long vm=sc(SYS_ioctl,fd,0xB201,0,0); w("create_vm: ",11); hx(vm);
    for (int t=0;t<3;t++) {
        long r=sc(SYS_ioctl,vm,0x4010B241,(long)nm,0); w("setfw: ",7); hx(r);
        if (r==0) break;
        sleep_ms(200);
    }
    long cnt=sc(SYS_ioctl,vm,0xB243,0,0); w("vcpu_count: ",12); hx(cnt);
    for (int z=0;z<16;z++) back[z]=0;
    long g=sc(SYS_ioctl,vm,0x8010B242,(long)back,0); w("getfw: ",7); hx(g);
    w("name=",5); w(back,16); w("\n",1);
    long vc=sc(SYS_ioctl,vm,0xB240,0,0); w("create_vcpu: ",13); hx(vc);
    if (vc>=0) { for (int z=0;z<4096;z++) run[z]=0;
        long rr=sc(SYS_ioctl,vc,0xB280,(long)run,0); w("run: ",5); hx(rr); }
    sc(SYS_exit,0,0,0,0); for(;;){}
}
