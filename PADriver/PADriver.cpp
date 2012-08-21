//===- PADriver.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "padriver"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/InstrTypes.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CallSite.h"
#include "llvm/ADT/Statistic.h"
#include "PointerAnalysis.h"
using namespace llvm;

STATISTIC(PABaseCt,  "Counts number of base constraints");
STATISTIC(PAAddrCt,  "Counts number of address constraints");
STATISTIC(PALoadCt,  "Counts number of load constraints");
STATISTIC(PAStoreCt, "Counts number of store constraints");

namespace {
    struct PADriver : public ModulePass {
        static char ID; // Pass identification, replacement for typeid
        PointerAnalysis* pointerAnalysis;

        // Used to assign a int ID to Values
        int currInd;
        std::map<Value*, int> valMap;
        std::map<Value*, std::vector<int> > valMem;

        // Constructor
        PADriver() : ModulePass(ID) { 
            pointerAnalysis = new PointerAnalysis();
            currInd = 0;

            PAAddrCt = 0;
            PABaseCt = 0;
            PALoadCt = 0;
            PAStoreCt = 0;
        }

        // Get a (possibly new) int ID associated with 
        // the given Value
        int Value2Int(Value *v) {
            if (valMap.count(v))
                return valMap[v];

            int n = ++currInd;
            valMap[v] = n;

            return n;
        }

        // Get the int ID of a new memory space
        int getNewMem()
        {
            return ++currInd;
        }

        virtual bool runOnModule(Module &M) {
            Module::iterator F;

            // Iterate on the instructions, calling the appropriate functions of the
            // Pointer Analysis to each instruction type.
            for (F = M.begin(); F != M.end(); F++) {
                for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
                    for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {

                        // Function Call
                        if (isa<CallInst>(I)) {
                            CallInst *CI = dyn_cast<CallInst>(I);

                            if (CI) {
                                Function *FF = CI->getCalledFunction();

                                // MALLOC
                                if (FF && FF->getName() == "malloc") {

                                    std::vector<int> mems;
                                    if (valMem.find(I) == valMem.end()) 
                                    {
                                        mems.push_back(getNewMem());
                                        valMem[I] = mems;
                                    }
                                    else
                                    {
                                        mems = valMem[I];
                                    }

                                    int a = Value2Int(I);
                                    int b = mems[0];
                                    errs() << "MALLOC: "<< a <<" = &"<< b <<"\n";
                                    pointerAnalysis->addAddr(a, b);
                                    PAAddrCt++;
                                }
                            }
                        }

                        // Handle special operations
                        switch (I->getOpcode()) {
                            case Instruction::Alloca:
                                {
                                    AllocaInst *AI = dyn_cast<AllocaInst>(I);
                                    const Type *Ty = AI->getAllocatedType();

                                    std::vector<int> mems;
                                    unsigned numElems = 1;
                                    bool isStruct = false;

                                    if (Ty->isStructTy()) { // Handle structs
                                        const StructType *StTy = dyn_cast<StructType>(Ty);
                                        numElems = StTy->getNumElements();
                                        isStruct = true;
                                    }

                                    if (valMem.find(I) == valMem.end()) {
                                        for (unsigned i = 0; i < numElems; i++) {
                                            mems.push_back(getNewMem());
                                        }

                                        valMem[I] = mems;
                                    } else {
                                        mems = valMem[I];
                                    }

                                    for (unsigned i = 0; i < mems.size(); i++) {
                                        int a = Value2Int(I);
                                        pointerAnalysis->addAddr(a, mems[i]);
                                        PAAddrCt++;
                                        errs() << "Alloca: "<< a <<" = &"<< mems[i] <<"\n";
                                    }

                                    break;
                                }
                            case Instruction::GetElementPtr:
                                {
                                    GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(I);
                                    Value *v = GEPI->getPointerOperand();
                                    const PointerType *PoTy = GEPI->getPointerOperandType();
                                    const Type *Ty = PoTy->getElementType();

                                    if (Ty->isStructTy() && valMem.count(v)) {
                                        int i = 0;
                                        unsigned pos = 0;

                                        for (User::op_iterator it = GEPI->idx_begin(), e = GEPI->idx_end(); it != e; ++it) {
                                            if (i == 1)
                                                pos = cast<ConstantInt>(*it)->getZExtValue();

                                            i++;
                                        }

                                        std::vector<int> mems = valMem[v];
                                        int a = Value2Int(I);
                                        pointerAnalysis->addBase(a, mems[pos]);
                                        PABaseCt++;
                                    } else {
                                        int a = Value2Int(I);
                                        int b = Value2Int(v);
                                        pointerAnalysis->addBase(a, b);
                                        PABaseCt++;
                                    }

                                    errs() << "GetElementPtr\n";
                                    break;
                                }
                            case Instruction::BitCast:
                                {
                                    Value *v = I->getOperand(0);

                                    if (v->getType()->isPointerTy()) {
                                        int a = Value2Int(I);
                                        int b = Value2Int(v);
                                        pointerAnalysis->addBase(a, b);
                                        errs() << "BitCast: "<<a<<" = "<<b<<"\n";
                                        PABaseCt++;
                                    }

                                    break;
                                }
                            case Instruction::Store:
                                {
                                    // *ptr = v
                                    StoreInst *SI = dyn_cast<StoreInst>(I);
                                    Value *v = SI->getValueOperand();
                                    Value *ptr = SI->getPointerOperand();

                                    if (v->getType()->isPointerTy()) {
                                        int a = Value2Int(ptr);
                                        int b = Value2Int(v);
                                        errs() << "Store: *"<<a<<" = "<<b<<"\n";
                                        pointerAnalysis->addStore(a, b);
                                        PAStoreCt++;
                                    }

                                    break;
                                }
                            case Instruction::Load:
                                {
                                    // I = *ptr
                                    LoadInst *LI = dyn_cast<LoadInst>(I);
                                    Value *ptr = LI->getPointerOperand();

                                    int a = Value2Int(I);
                                    int b = Value2Int(ptr);
                                    pointerAnalysis->addLoad(a, b);
                                    errs() << "Load: "<<a<<" = *"<<b<<"\n";
                                    PALoadCt++;
                                    break;
                                }
                        }
                    }
                }

            }

            // Run the analysis and print the output
            pointerAnalysis->solve();
            pointerAnalysis->print();
            return false;
        }
    };
}

// Register the pass to the LLVM framework
char PADriver::ID = 0;
static RegisterPass<PADriver> X("pa", "Pointer Analysis Driver Pass");
