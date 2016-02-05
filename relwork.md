# Related Works #

### Dynamic instrumentation ###

  * Dynamic Taint Analysis for Automatic Detection, Analysis, and Signature Generation of Exploits on Commodity Software, _James Newsome_, NDSS 2005
    * **Summary**: the paper describes a dynamic instrumentation technique that is more efficient and precise than the state-of-the-art approaches in 2005.
    * **What can we take from it?** We should use some optimization techniques to mitigate the impact of our instrumentation.

  * Dytan: A Generic Dynamic Taint Analysis Framework, _James Clause, Wanchun Li, and Alessandro Orso_, ISSTA 2007
    * **Summary**: the paper describes a general framework for dynamic taint analysis that is flexible, customizable, performs both data-flow and control-flow based taint analysis and works at the application level directly on binaries with on-the-fly instrumentation. The control-flow based propagation computes CFGs and pdom information statically to further use it dynamically. With this framework they simulated two previous ad-hoc dynamic taint analysis with little configuration effort and obtained similar results. The time overhead for data-flow based propagation alone was ~30x, whereas the overhead for data- and control-flow propagation was ~50x. (using Intel's PIN)
    * **What can we take from it?** They reference works that achieve dynamic taint analysis by instrumenting the program's source code (Xu et al. 06 and Lam and Chiueh 06), use dynamic taint analysis to enforce information-flow policies (Vachharajani et al. 04, Chow et al. 04, McCamant and Ernst 06) and perform static information-flow analysis (Myers 99, Pottier et al. 02). We could consider using control-flow based propagation to improve our results.

### Combining static and dynamic analysis to secure programs ###

  * Combining Static and Dynamic Analysis to Discover Software Vulnerabilities, _Zhang et al._, IMIS'11
    * **Summary**: the paper describes a way to get more from the dynamic analysis. Basically, the authors run the binary program, searching for tainted flow vulnerabilities. But the authors also use the dynamic analysis to build the control flow graph of the program. This CFG lets then run static analysis on the program; hence, checking the parts of it that have not been verified by the dynamic analysis.
    * **What can we take from it?** The approach that the authors use to combine static and dynamic analysis is very different than the approach that we use. They do not use the static analysis to decrease the performance overhead of the dynamic analysis. Instead, they use these two analysis in a complementary way, so that one can obtain more information about the program.

  * Securing Web Application Code by Static Analysis and Runtime Protection, _Huang et al._, WWW '04
    * **Summary**: the paper describes a lattice-based static analysis algorithm derived from type systems to detect vulnerable parts of code in Web applications (PHP) to instrument with runtime guards. The experimental results shown that using the static analysis to discover vulnerable points instead of instrumenting all the sensitive function calls reduced potential runtime overhead by 98.4%. For the static analysis they got a false positive rate of 29.9% for the simple lattice and 26.9% adding support for type-aware qualifiers.
    * **What can we take from it?** The static analysis can be used to reduce the amount of instrumented code and thus reduce the runtime overhead of our solution.

  * Saner: Composing Static and Dynamic Analysis to Validate Sanitization in Web Applications, _Balzarotti et al._, S&P'08
    * **Summary**: This paper combines static and dynamic analysis to increase the precision of bug detection. The authors use the dynamic analysis to check the possible vulnerabilities reported by the static analysis.
    * **What can we take from it**: the related work section is very comprehensive, and there was no work in that section that combines dynamic and static analyses in the way that we do it.


  * Program Slicing Enhances a Verification Technique Combining Static and Dynamic Analysis, _Chebaro et al._, SAC'12
    * **Summary**: this paper is similar to Balzarotti's. It uses static analysis to guide a program slicer. The authors have used a tainted-flow-like analysis to reduce the program to only those parts that are necessary to send data from source to sink functions. Then, they feed this reduced program to a test generator, which tries to prove that some vulnerability exists.
    * **What can we take from it**: the authors develop a nice theory explaining what is a slice, and how the static analysis can lead to this slice.

  * The MINESTRONE Architecture Combining Static and Dynamic Analysis Techniques for Software Security, _Keromytis et al._

### Statically tracking the flow of information ###

  * JFlow: Practical Mostly-Static Information Flow Control, _Myers_, POPL'99

  * Information Flow Inference for ML, _Pottier et al._, POPL'02

### Other ###

  * Framework for Instruction-level Tracing and Analysis of Program Executions, _Bhansali et al._, VEE 2006
    * **Summary**: the paper presents a framework to collect detailed traces of a program's execution that can be re-simulated deterministically. The framework has two components: a dynamic binary translator and a trace recorder. It does not require any static instrumentation of the program. They measured an overhead of 12-17x slowdown.
    * **What can we take from it?** We can see that the measured overhead is high and does not include the time to perform a dynamic taint analysis on-the-fly or over the produced run traces, so adopting a similar approach could not produce an overhead of at most 5% as we want.

  * A comparison of publicly available tools for dynamic buffer overflow prevention, _Wilander et al._
    * **Summary**: the authors implement a testbed of 20 buffer overflow attacks and use it to compare empirically and theoretically four publicly available tools for dynamic intrusion prevention. The tools were StackGuard, Stack Shield, ProPolice and Libsafe/Libvefify. They found that the best tool could stop only 50% of the attacks and six attack forms were not handled by any tool. In their opinions, the poor results obtained are due the tested tools all aim to protect known attack targets such as the return address, not all targets.
    * **What can we take from it?** We could analyze the usefulness of an address leakage for each one of the buffer overflow attacks of the testbed when ASLR (and DEP) is enabled. We could also argue that an address leakage can be useful to bypass one of such protections based on stack canaries or separated stacks. One of the tested tool (Libsafe) combines static and dynamic analysis to prevent buffer overflows.