#include "cnufftspread.h"
#include "besseli.h"

#include "qute.h"
#include <stdlib.h>
#include <vector>
#include <math.h>

std::vector<long> compute_sort_indices(long M,double *kx, double *ky, double *kz,long N1,long N2,long N3);
double evaluate_kernel(double x,const cnufftspread_opts &opts);
std::vector<double> compute_kernel_values(double frac1,double frac2,double frac3,const cnufftspread_opts &opts);

bool cnufftspread(
        long N1, long N2, long N3, double *data_uniform,
        long M, double *kx, double *ky, double *kz, double *data_nonuniform,
        cnufftspread_opts opts
) {

    opts.private_KB_W=opts.nspread*opts.KB_fac1;
    double tmp0=opts.private_KB_W*opts.private_KB_W/4-0.8;
    if (tmp0<0) tmp0=0; //fix this?
    opts.private_KB_beta=M_PI*sqrt(tmp0)*opts.KB_fac2;

    // Sort the data
    std::vector<double> kx2(M),ky2(M),kz2(M),data_nonuniform2(M*2);
    std::vector<long> sort_indices(M);
    if (opts.sort_data)
        sort_indices=compute_sort_indices(M,kx,ky,kz,N1,N2,N3);
    else {
        for (long i=0; i<M; i++)
            sort_indices[i]=i;
    }
    for (long i=0; i<M; i++) {
        long jj=sort_indices[i];

        kx2[i]=kx[jj];
        ky2[i]=ky[jj];
        kz2[i]=kz[jj];
        if (opts.spread_direction==1) {
            data_nonuniform2[i*2]=data_nonuniform[jj*2];
            data_nonuniform2[i*2+1]=data_nonuniform[jj*2+1];
        }
    }

    int spread1=-opts.nspread/2;
    int spread2=spread1+opts.nspread-1;

    for (long i=0; i<N1*N2*N3; i++) {
        data_uniform[i*2]=0;
        data_uniform[i*2+1]=0;
    }
    long R=opts.nspread;
    for (long i=0; i<M; i++) {
        long i1=(long)((kx2[i]+0.5));
        long i2=(long)((ky2[i]+0.5));
        long i3=(long)((kz2[i]+0.5));
        double frac1=kx2[i]-i1;
        double frac2=ky2[i]-i2;
        double frac3=kz2[i]-i3;
        std::vector<double> kernel_values=compute_kernel_values(frac1,frac2,frac3,opts);

        //for (int dz=spread1; dz<=spread2; dz++) {
            if (opts.spread_direction==1) {
                double re0=data_nonuniform2[i*2];
                double im0=data_nonuniform2[i*2+1];
                for (int dz=spread1; dz<=spread2; dz++) {
                    long j3=i3+dz;
                    if ((0<=j3)&&(j3<N3)) {
                        for (int dy=spread1; dy<=spread2; dy++) {
                            long j2=i2+dy;
                            if ((0<=j2)&&(j2<N2)) {
                                for (int dx=spread1; dx<=spread2; dx++) {
                                    long j1=i1+dx;
                                    if ((0<=j1)&&(j1<N1)) {
                                        double kern0=kernel_values[(dx-spread1)+R*(dy-spread1)+R*R*(dz-spread1)];
                                        long jjj=j1+N1*j2+N1*N2*j3;
                                        data_uniform[jjj*2]+=re0*kern0;
                                        data_uniform[jjj*2+1]+=im0*kern0;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else {
                double re0=0;
                double im0=0;
                for (int dz=spread1; dz<=spread2; dz++) {
                    long j3=i3+dz;
                    if ((0<=j3)&&(j3<N3)) {
                        for (int dy=spread1; dy<=spread2; dy++) {
                            long j2=i2+dy;
                            if ((0<=j2)&&(j2<N2)) {
                                for (int dx=spread1; dx<=spread2; dx++) {
                                    long j1=i1+dx;
                                    if ((0<=j1)&&(j1<N1)) {
                                        double kern0=kernel_values[(dx-spread1)+R*(dy-spread1)+R*R*(dz-spread1)];
                                        long jjj=j1+N1*j2+N1*N2*j3;
                                        re0+=data_uniform[jjj*2]*kern0;
                                        im0+=data_uniform[jjj*2+1]*kern0;
                                    }
                                }
                            }
                        }
                    }
                }
                data_nonuniform2[i*2]=re0;
                data_nonuniform2[i*2+1]=im0;
            }
        //}
    }

    if (opts.spread_direction==2) {
        for (long i=0; i<M; i++) {
            long jj=sort_indices[i];
            data_nonuniform[jj*2]=data_nonuniform2[i*2];
            data_nonuniform[jj*2+1]=data_nonuniform2[i*2+1];
        }
    }

    return true;
}

std::vector<double> compute_kernel_values(double frac1,double frac2,double frac3,const cnufftspread_opts &opts) {
    long R=opts.nspread;
    int nspread1=-opts.nspread/2;
    std::vector<double> vals1(R),vals2(R),vals3(R);
    for (int i=0; i<R; i++) {
        vals1[i]=evaluate_kernel(frac1-(i+nspread1),opts);
        vals2[i]=evaluate_kernel(frac2-(i+nspread1),opts);
        vals3[i]=evaluate_kernel(frac3-(i+nspread1),opts);
    }

    std::vector<double> ret(R*R*R);
    long aa=0;
    for (int k=0; k<R; k++) {
        double val3=vals3[k];
        for (int j=0; j<R; j++) {
            double val2=val3*vals2[j];
            for (int i=0; i<R; i++) {
                double val1=val2*vals1[i];
                ret[aa]=val1;
                aa++;
            }
        }
    }

    return ret;
}

double evaluate_kernel(double x,const cnufftspread_opts &opts) {
    double tmp1=1-(2*x/opts.private_KB_W)*(2*x/opts.private_KB_W);
    if (tmp1<0) {
        return 0;
    }
    else {
        double y=opts.private_KB_beta*sqrt(tmp1);
        //return besseli0(y);
        return besseli0_approx(y);
    }
}

std::vector<long> compute_sort_indices(long M,double *kx, double *ky, double *kz,long N1,long N2,long N3) {
    //Q_UNUSED(N1)
    //Q_UNUSED(kx)

    std::vector<long> counts(N2*N3);
    for (long j=0; j<N2*N3; j++)
        counts[j]=0;
    for (long i=0; i<M; i++) {
        long i2=(long)(ky[i]+0.5);
        if (i2<0) i2=0;
        if (i2>=N2) i2=N2-1;

        long i3=(long)(kz[i]+0.5);
        if (i3<0) i3=0;
        if (i3>=N3) i3=N3-1;

        counts[i2+N2*i3]++;
    }
    std::vector<long> inds(N2*N3);
    long offset=0;
    for (long j=0; j<N2*N3; j++) {
        inds[j]=offset;
        offset+=counts[j];
    }

    std::vector<long> ret_inv(M);
    for (long i=0; i<M; i++) {
        long i2=(long)(ky[i]+0.5);
        if (i2<0) i2=0;
        if (i2>=N2) i2=N2-1;

        long i3=(long)(kz[i]+0.5);
        if (i3<0) i3=0;
        if (i3>=N3) i3=N3-1;

        long jj=inds[i2+N2*i3];
        inds[i2+N2*i3]++;
        ret_inv[i]=jj;
    }

    std::vector<long> ret(M);
    for (long i=0; i<M; i++) {
        ret[ret_inv[i]]=i;
    }

    return ret;
}

void set_kb_opts_from_kernel_params(cnufftspread_opts &opts,double *kernel_params) {
    opts.nspread=kernel_params[1];
    opts.KB_fac1=kernel_params[2];
    opts.KB_fac2=kernel_params[3];
}

void set_kb_opts_from_eps(cnufftspread_opts &opts,double eps) {
    int nspread=12; double fac1=1,fac2=1;
    if (eps>=1e-2) {
        nspread=4; fac1=0.75; fac2=1.71;
    }
    else if (eps>=1e-4) {
        nspread=6; fac1=0.83; fac2=1.56;
    }
    else if (eps>=1e-6) {
        nspread=8; fac1=0.89; fac2=1.45;
    }
    else if (eps>=1e-8) {
        nspread=10; fac1=0.90; fac2=1.47;
    }
    else if (eps>=1e-10) {
        nspread=12; fac1=0.92; fac2=1.51;
    }
    else if (eps>=1e-12) {
        nspread=14; fac1=0.94; fac2=1.48;
    }
    else {
        nspread=16; fac1=0.94; fac2=1.46;
    }

    opts.nspread=nspread;
    opts.KB_fac1=fac1;
    opts.KB_fac2=fac2;
}

void cnufftspread_type1(int N,double *Y,int M,double *kx,double *ky,double *kz,double *X,double *kernel_params) {
    cnufftspread_opts opts;
    set_kb_opts_from_kernel_params(opts,kernel_params);
    opts.spread_direction=1;

    cnufftspread(N,N,N,Y,M,kx,ky,kz,X,opts);
}