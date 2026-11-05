// Test-only SAN composition: all other arguments enter the unchanged app main.
#include <cstring>
int retopo_render_tests_main(int,char **);
extern "C" int __real_main(int,char **);
extern "C" int __wrap_main(int argc,char **argv){
 if(argc==3&&std::strcmp(argv[1],"--retopo-render-tests")==0){
  char *test_argv[]{argv[0],argv[2],nullptr};return retopo_render_tests_main(2,test_argv);
 }
 return __real_main(argc,argv);
}
