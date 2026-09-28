// C++14, standard library only. Finite low-excess bridge search.
// This is NOT an exact torus solver: every printed witness is certified.
// Absence through a finite depth is not a nonexistence proof. An optional
// full-map stabilization certificate upgrades the BRIDGE search to all lengths.
// All-length extension: inspect odd and even bridge endpoints.
// Usage: bridge_all_lengths [k=2] [depth=16] [budget=2] [state_cap=400000]
// m=5*k+2. All column counts allowed by the energy bounds are searched.
// No Python, external solver, compiler-specific intrinsics, or library needed.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using Mask = std::uint32_t;
using Key = std::uint64_t;
int pop(Mask x) { int n=0; while(x) { x &= x-1; ++n; } return n; }
int mod(int a, int b) { a%=b; return a<0 ? a+b : a; }

struct Node { Mask a,b; int cost,parent; };
struct Result { int cost,n,length,index; };

struct Search {
    int k,m,depth,budget,c; std::size_t cap; Mask full,A=1,B=0;
    std::vector<std::vector<Node>> layers;
    std::map<int,Result> best;
    std::unordered_map<Key,int> ground;
    explicit Search(int kk,int dd,int bb,std::size_t cc):
        k(kk),m(0),depth(dd),budget(bb),c(0),cap(cc) {
        if(k<1 || k>5 || depth<1 || budget<0 || budget>6 || cap<1)
            throw std::runtime_error("Require 1<=k<=5, depth>=1, 0<=budget<=6, cap>=1");
        m=5*k+2;c=2*k+1;
        full=(Mask(1)<<m)-1;
        for(int h=0;h<k;++h) A |= Mask(1)<<(2+5*h);
        for(int h=1;h<=k;++h) B |= Mask(1)<<(5*h);
        for(int s=0;s<m;++s) ground[key(shift(A,s),shift(B,s))]=s;
    }
    Mask shift(Mask a,int s) const {
        s=mod(s,m); if(!s) return a;
        return ((a<<s)|(a>>(m-s)))&full;
    }
    Mask vertical(Mask a) const { return a|shift(a,1)|shift(a,-1); }
    Key key(Mask a,Mask b) const { return (Key(a)<<m)|b; }
    // Enumerate all subsets of precisely 'need' bits, without floating point.
    template<class F> void subsets(Mask available,int need,Mask chosen,F &emit) const {
        int count=pop(available);
        if(need<0 || need>count) return;
        if(need==0) { emit(chosen); return; }
        if(need==count) { emit(chosen|available); return; }
        while(available) {
            Mask bit=available & (Mask(0)-available);
            available ^= bit;
            subsets(available,need-1,chosen|bit,emit);
        }
    }
    // Exact nonnegative local term: S + Cp + Cv + R0 + R1 + G.
    // With the middle-column hole count h, t=a+3b+d-m and G=h+min(t,0).
    int structural(int aa,int bb,int dd) const {
        int sa=(aa<=k), sb=(bb<=k), sd=(dd<=k);
        int p=std::max(0,bb-k-1),v=std::max(0,k-bb),t=aa+3*bb+dd-m;
        return (sb==sd)+p*(2-sa-sd)+v*(1+sa+sd)
            +(sb ? std::max(t,0) : std::max(-t,0));
    }
    std::vector<Mask> path(int length,int index) const {
        std::vector<Mask> rev;
        for(int j=length;j>=1;--j) {
            const Node &v=layers[j][index]; rev.push_back(v.b); index=v.parent;
        }
        std::reverse(rev.begin(),rev.end());
        std::vector<Mask> out={A,B}; out.insert(out.end(),rev.begin(),rev.end());
        return out;
    }
    std::vector<Mask> torus(int length,int index,int phase) const {
        auto out=path(length,index); out.resize(out.size()-2);
        int pairs=mod(-phase,m);
        if(static_cast<int>(out.size())+2*pairs<3) pairs+=m;
        for(int j=0;j<pairs;++j) {
            out.push_back(shift(A,phase+j)); out.push_back(shift(B,phase+j));
        }
        return out;
    }
    // Independent direct vertex coverage/weight audit, not the transfer cost.
    int audit(const std::vector<Mask>& cols,bool print=false) const {
        int n=static_cast<int>(cols.size()),weight=0;
        if(n<3) throw std::runtime_error("Torus length below three");
        for(int j=0;j<n;++j) {
            Mask holes=full&~(vertical(cols[j])|cols[(j+n-1)%n]|cols[(j+1)%n]);
            if(holes&cols[j]) throw std::runtime_error("A center was labelled 1");
            weight+=2*pop(cols[j])+pop(holes);
            if(print) {
                std::cout << "  column " << j << " centers:";
                for(int i=0;i<m;++i) if(cols[j]&(Mask(1)<<i)) std::cout << ' ' << i;
                std::cout << "; ones:";
                for(int i=0;i<m;++i) if(holes&(Mask(1)<<i)) std::cout << ' ' << i;
                std::cout << '\n';
            }
        }
        return weight-c*n;
    }
    void run() {
        layers.push_back({{A,B,0,-1}});
        std::cout << "All-length finite bridge search: m="<<m<<", budget="<<budget
                  <<", maximum depth="<<depth<<". All column counts permitted by the global energy bounds included.\n";
        for(int length=1;length<=depth;++length) {
            std::vector<Node> next;
            std::unordered_map<Key,int> idx;
            bool aborted=false;
            const auto &prev=layers.back();
            for(int pi=0;pi<static_cast<int>(prev.size());++pi) {
                const Node &v=prev[pi];
                int aa=pop(v.a),bb=pop(v.b);
                Mask covered=vertical(v.b)|v.a,missing=full^covered;
                int missingCount=pop(missing);
                int low=std::max(0,k-budget),high=std::min(m,k+1+3*budget/2);
                for(int dd=low;dd<=high;++dd) {
                    int base=structural(aa,bb,dd),t=aa+3*bb+dd-m;
                    if(base>budget-v.cost) continue;
                    int minH=std::max(0,missingCount-dd);
                    minH=std::max(minH,std::max(0,-t));
                    int maxH=std::min(missingCount,budget-v.cost-base-std::min(t,0));
                    for(int h=minH;h<=maxH;++h) {
                        int extras=dd-(missingCount-h);
                        if(extras<0 || extras>pop(covered)) continue;
                        auto omitFn=[&](Mask omitted) {
                            Mask kept=missing^omitted;
                            auto extraFn=[&](Mask extra) {
                                Mask d=kept|extra;
                                int cost=v.cost+base+h+std::min(t,0);
                                if(cost<0 || cost>budget) throw std::runtime_error("Invalid local cost");
                                Key ky=key(v.b,d);
                                auto it=idx.find(ky);
                                if(it==idx.end()) {
                                    if(next.size()>=cap) { aborted=true; return; }
                                    idx.emplace(ky,static_cast<int>(next.size()));
                                    next.push_back({v.b,d,cost,pi});
                                } else if(cost<next[it->second].cost) next[it->second]={v.b,d,cost,pi};
                            };
                            subsets(covered,extras,0,extraFn);
                        };
                        subsets(missing,h,0,omitFn);
                    }
                    if(aborted) break;
                }
                if(aborted) break;
            }
            if(aborted) {
                std::cout<<"STOP: state cap reached at depth "<<length
                         <<"; this entire incomplete layer is discarded.\n"; break;
            }
            layers.push_back(std::move(next));
            std::cout<<"depth "<<length<<": "<<layers.back().size()<<" states";
            { // Inspect terminal standard pairs at every length, including odd lengths.
                for(int i=0;i<static_cast<int>(layers.back().size());++i) {
                    const Node &v=layers.back()[i]; auto g=ground.find(key(v.a,v.b));
                    if(g==ground.end()) continue;
                    int s=g->second,residue=mod(length-2*s,2*m);
                    auto cols=torus(length,i,s); int actual=audit(cols);
                    if(actual!=v.cost) throw std::runtime_error("Energy differs from direct witness audit");
                    int n=static_cast<int>(cols.size()); auto found=best.find(residue);
                    if(found==best.end() || std::make_pair(actual,n)<std::make_pair(found->second.cost,found->second.n)) {
                        best[residue]={actual,n,length,i};
                        std::cout<<" [residue="<<residue<<", E="<<actual<<", n="<<n<<"]";
                    }
                }
            }
            std::cout<<std::endl;
            // Translation-equivariance of the transition operator propagates
            // this exact identity to every subsequent layer by induction.
            // Compare all reachable pairs AND their minimum costs, not sizes.
            if(length>=3 && length%2==0 && layers[length-2].size()==layers[length].size()) {
                bool certified=true;
                for(const Node &old:layers[length-2]) {
                    auto it=idx.find(key(shift(old.a,1),shift(old.b,1)));
                    if(it==idx.end() || layers[length][it->second].cost!=old.cost) {
                        certified=false; break;
                    }
                }
                if(certified) {
                    std::cout<<"CERTIFICATE: F_"<<length<<"(shift(pair,1)) = F_"<<length-2
                             <<"(pair), all "<<layers[length].size()<<" reachable states and costs checked.\n"
                             <<"Translation-equivariance proves all later layers by induction.\n"
                             <<"The listed residues exhaust budget-at-most-"<<budget
                             <<" positive-to-positive bridges at this fixed width (all length parities).\n";
                    break;
                }
            }
            if(layers.back().empty()) break;
        }
        for(const auto &item:best) {
            const Result &r=item.second; const Node &v=layers[r.length][r.index];
            int s=ground.at(key(v.a,v.b)); auto cols=torus(r.length,r.index,s);
            std::cout<<"WITNESS residue="<<item.first<<" m="<<m<<" n="<<r.n
                     <<" weight="<<c*r.n+r.cost<<" E="<<r.cost<<'\n';
            audit(cols,true);
        }
        std::cout<<"All displayed witnesses verified directly. Without a CERTIFICATE, missing residues are not excluded.\n"
                 <<"Even a bridge certificate alone does not exclude tori containing no standard adjacent pair.\n";
    }
};

int main(int argc,char**argv) {
    try {
        int k=argc>1?std::stoi(argv[1]):2,depth=argc>2?std::stoi(argv[2]):16;
        int budget=argc>3?std::stoi(argv[3]):2;
        std::size_t cap=argc>4?static_cast<std::size_t>(std::stoull(argv[4])):400000;
        Search(k,depth,budget,cap).run();
    } catch(const std::exception &e) { std::cerr<<"ERROR: "<<e.what()<<'\n'; return 1; }
}
