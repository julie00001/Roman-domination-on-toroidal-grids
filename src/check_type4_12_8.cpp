// C++14 exhaustive check of the remaining type-4 cluster on T_{12,8}.
// Profile: H,P,Y,X,Z,R,K,L, center counts 3,2,2,4,2,2,3,2;
// labels 1 occur once in P and once in R. Q is 1,0,0,4,0,0,1,0.
// The regular L forces H and K to be canonical heavy sets by the
// manuscript's local rigidity lemmas; normalize H=A by a row translation.
// This script checks this profile only, not the entire Roman domination number.
#include <cstdint>
#include <iostream>
#include <vector>
using Mask=std::uint32_t;
const int M=12; const Mask FULL=(Mask(1)<<M)-1;
int pop(Mask x) { int n=0;while(x){x&=x-1;++n;}return n; }
Mask shift(Mask x,int s) { s=(s%M+M)%M;return s?((x<<s)|(x>>(M-s)))&FULL:x; }
Mask vert(Mask x) { return x|shift(x,1)|shift(x,-1); }
struct Half { Mask p,y,x; };
void print_mask(Mask a) {
    std::cout<<'{';bool first=true;
    for(int i=0;i<M;++i) if(a&(Mask(1)<<i)) {
        if(!first) std::cout<<',';
        std::cout<<i;first=false;
    }
    std::cout<<'}';
}
std::vector<Half> halves(Mask heavy,const std::vector<Mask>& lights) {
    std::vector<Half> out;
    for(Mask p:lights) {
        if(vert(p)&heavy) continue;
        for(Mask y:lights) {
            if(y&(vert(p)|heavy)) continue;
            // The outer light has exactly one hole (3+6+2 distinct rows).
            if(pop(FULL&~(heavy|vert(p)|y))!=1) continue;
            // Inner light has Q=0, so the other two centers cannot overlap
            // its three-point vertical neighborhoods.
            if(p&vert(y)) continue;
            Mask x=FULL&~(vert(y)|p);
            if(pop(x)==4) out.push_back({p,y,x});
        }
    }
    return out;
}
int main() {
    Mask A=1|(1<<2)|(1<<7);
    std::vector<Mask> lights;
    for(Mask a=0;a<=FULL;++a) if(pop(a)==2 && pop(vert(a))==6) lights.push_back(a);
    auto left=halves(A,lights);
    for(const Half &a:left) {
        std::cout<<"HALF P=";print_mask(a.p);std::cout<<" Y=";print_mask(a.y);
        std::cout<<" X=";print_mask(a.x);std::cout<<'\n';
    }
    std::uint64_t regularTriples=0,candidatePairs=0,valid=0;
    for(int d=0;d<M;++d) {
        Mask K=shift(A,d);auto right=halves(K,lights);
        for(Mask L:lights) {
            if((A&K)||(A&vert(L))||(K&vert(L))||(A|K|vert(L))!=FULL) continue;
            ++regularTriples;
            std::cout<<"OUTER d="<<d<<" L=";print_mask(L);std::cout<<'\n';
            for(const Half &a:left) for(const Half &b:right) {
                if(a.x!=b.x) continue;
                ++candidatePairs;
                std::vector<Mask> cols={A,a.p,a.y,a.x,b.y,b.p,K,L};
                int weight=0;bool ok=true;
                for(int j=0;j<8;++j) {
                    int holes=pop(FULL&~(vert(cols[j])|cols[(j+7)%8]|cols[(j+1)%8]));
                    if(holes!=((j==1||j==5)?1:0)) ok=false;
                    weight+=2*pop(cols[j])+holes;
                }
                if(ok && weight==42) { ++valid;std::cout<<"VALID phase="<<d<<'\n'; }
            }
        }
    }
    std::cout<<"Separated light masks: "<<lights.size()<<"\nLeft half-clusters: "<<left.size()
             <<"\nRegular outer triples: "<<regularTriples<<"\nMatching central masks: "<<candidatePairs
             <<"\nValid type-4 configurations after normalization: "<<valid<<'\n';
    return valid?1:0;
}
