# Detecting and Preventing Address Leaks #

An address leak is a software vulnerability that allows an adversary to discover where a program is loaded in memory. Although seemingly harmless, this information gives the adversary the means to circumvent two widespread protection mechanisms: Address Space Layout Randomization (ASLR) and Data Execution Prevention (DEP). The goal of this project is to develop static and dynamic analyses techniques that detect address disclosures vulnerabilities, and prevents them from happening. We have developed two main techniques:
  * First, a dynamic analysis based on an instrumentation library. This dynamic analysis prevents address disclosure at runtime.
  * Secondly, a static analysis technique that detects the possibility of address information leaking to the outside world.

The combination of the static and dynamic analyses provide us with a reliable and practical way to secure software against address leaks. For further information about this project, check the links below:
  * [Exploiting Address Leaks](Example1.md)
  * [Exploiting a Function Protected with Canaries](Example2.md)
  * [How to Use the Pointer Analysis](HowToUseThePointerAnalysis.md)
  * [Related Work](relwork.md)

This project is sponsored by the Brazilian Research Council: [CNPq](http://www.cnpq.br/).