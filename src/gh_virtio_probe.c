#define SYS_openat 56
#define SYS_ioctl  29
#define SYS_write  64
#define SYS_exit   93
#define SYS_fork   220
#define SYS_mmap   222
#define SYS_eventfd2 19
#define AT_FDCWD -100
#define O_RDWR 2
static long sc(long n,long a,long b,long c,long d){register long x8 __asm__("x8")=n;register long x0 __asm__("x0")=a;register long x1 __asm__("x1")=b;register long x2 __asm__("x2")=c;register long x3 __asm__("x3")=d;__asm__ volatile("svc #0":"+r"(x0):"r"(x8),"r"(x1),"r"(x2),"r"(x3):"memory");return x0;}
static long mmap6(long addr,long len,long prot,long flags,long fd,long off){
    register long x8 __asm__("x8")=SYS_mmap; register long x0 __asm__("x0")=addr; register long x1 __asm__("x1")=len;
    register long x2 __asm__("x2")=prot; register long x3 __asm__("x3")=flags; register long x4 __asm__("x4")=fd; register long x5 __asm__("x5")=off;
    __asm__ volatile("svc #0":"+r"(x0):"r"(x8),"r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5):"memory"); return x0; }
static int outfd=-1; static const char outp[]="/data/local/tmp/o.txt";
static void w(const char*s,long n){ if(outfd<0) outfd=sc(SYS_openat,AT_FDCWD,(long)outp,0x241,0644); sc(SYS_write,outfd,(long)s,n,0); sc(82,outfd,0,0,0); }
static void hx(long v){char b[20];const char*d="0123456789abcdef";b[0]='0';b[1]='x';for(int i=0;i<16;i++)b[2+i]=d[(v>>((15-i)*4))&15];b[18]='\n';w(b,19);}
static const char dev[]="/dev/qgunyah";
static char nm[16]; static unsigned int s[4]; static unsigned char runbuf[4096];
#define LBL 16
void _start(void)
{
    const char *n="trustedvm"; int i=0; while(n[i]){nm[i]=n[i];i++;}
    if(outfd<0) outfd=sc(SYS_openat,AT_FDCWD,(long)outp,0x241,0644);
    long fd=sc(SYS_openat,AT_FDCWD,(long)dev,O_RDWR,0);
    long vm=sc(SYS_ioctl,fd,0xB201,0,0); w("vm ",3); hx(vm);
    if(sc(SYS_ioctl,vm,0x4010B241,(long)nm,0)) { w("setfw fail\n",11); goto out; }
    w("setfw ok\n",9);
    long c=sc(SYS_ioctl,vm,0xB243,0,0); w("vcpus ",6); hx(c);

    long vc0=-1; for(long k=0;k<c;k++){ long v=sc(SYS_ioctl,vm,0xB240,k,0); if(!k) vc0=v; }
    long pid=sc(SYS_fork,0,0,0,0);
    if(pid==0){ long rr=sc(SYS_ioctl,vc0,0xB280,(long)runbuf,0); sc(SYS_exit,0,0,0,0); for(;;){} }
    { struct { long s; long ns; } t = {0, 200000000}; sc(101,(long)&t,0,0,0); }
    w("vm started\n",11);
    s[0]=LBL; s[1]=0;
    long r=sc(SYS_ioctl,vm,0x4008B26D,(long)s,0); w("ack_reset ",10); hx(r);
    s[0]=LBL; s[1]=0; s[2]=0; s[3]=0;
    r=sc(SYS_ioctl,vm,0x4010B265,(long)s,0); w("set_feat ",9); hx(r);
    s[0]=LBL; s[1]=0; s[2]=0; s[3]=256;
    r=sc(SYS_ioctl,vm,0x4010B266,(long)s,0); w("set_qmax ",9); hx(r);

    long shm=0; if(!sc(SYS_ioctl,vm,0x8008B261,(long)&shm,0)){
        long p=mmap6(0, shm, 3, 1, vm, 0); w("mmap(off0) ",11); hx(p);
        if(p<0){ long p2=mmap6(0, shm, 3, 1, vm, (long)LBL<<12); w("mmap(lbl) ",10); hx(p2); }
    }
    long efd=sc(SYS_eventfd2,0,0,0,0); w("eventfd ",8); hx(efd);
    for(int f=0; f<2; f++){
      s[0]=LBL; s[1]=f; s[2]=(unsigned)efd; s[3]=0;
      r=sc(SYS_ioctl,vm,0x4010B263,(long)s,0); w("irqfd f=",9); hx(f); hx(r);
      s[0]=LBL; s[1]=f; s[2]=0; s[3]=(unsigned)efd;
      r=sc(SYS_ioctl,vm,0x4010B262,(long)s,0); w("ioev f=",7); hx(f); hx(r);
    }
    s[0]=17; s[1]=0; s[2]=(unsigned)efd; s[3]=0;
    r=sc(SYS_ioctl,vm,0x4010B263,(long)s,0); w("irqfd lbl17 ",12); hx(r);

    for(int k=0;k<4;k++){ s[0]=LBL; s[1]=s[2]=s[3]=0;
        long e=sc(SYS_ioctl,vm,0xC010B264,(long)s,0);
        w("wait ",5); hx(e); w(" ev ",4); hx(s[1]); w(" data ",6); hx(s[2]); }
out:
    sc(SYS_exit,0,0,0,0); for(;;){}
}
