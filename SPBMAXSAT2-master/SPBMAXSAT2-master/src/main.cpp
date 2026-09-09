#include "Solver/solver.h"
#include "Util/timer.h"
#include <signal.h>

using namespace std;

Solver s;

void interrupt(int sig)
{
	s.SimplePrint();
    _exit(10);
}

int main(int argc, char *argv[])
{
    signal(SIGTERM, interrupt);
    signal(SIGINT, interrupt);
    util::start_global_timer();
    s.BuildInstance(argv[1]);
    //s.originInstance.print_info();
    s.ParseParameters(argc, argv);
    cout << "c init time: " << util::global_elapsed_seconds() << " s" << endl;
    s.Solve();
    // s.OnlySearch();
    // cout << "c bestcost " << s.best_sol.cost << endl;
    s.SimplePrint();
    return 0;
}