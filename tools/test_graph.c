#include "csi_graph.h"
#include <assert.h>
int main(void) {
    assert(csi_graph_scale(120,10)==150);
    assert(csi_graph_scale(120,200)==220);
    assert(csi_graph_scale(5,10)==60);
    assert(csi_graph_y(0,150)==38);
    assert(csi_graph_y(120,150)==21);
    for(unsigned t=5;t<=2000;t+=5) for(unsigned peak=0;peak<=2000;peak+=10) {
        unsigned s=csi_graph_scale(t,peak);
        assert(s>=t && s>=peak && s>=60);
        int previous=38;
        for(unsigned v=0;v<=s;v+=5) {
            int y=csi_graph_y(v,s);assert(y>=16 && y<=38 && y<=previous); previous=y;
        }
    }
    assert(csi_graph_y(9999,100)==16);
    return 0;
}
