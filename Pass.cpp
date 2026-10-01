#include "llvm/Pass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include <set>


using namespace llvm;
using namespace llvm::cl;

static cl::opt<int> ResourceCount(
 "resources",
 cl::init(1),
 cl::desc("The number of resources for the list scheduler"));


static cl::opt<int> memoryports(
 "memoryports",
 cl::init(4),
 cl::desc("The number of memory ports for the list scheduler"));


namespace {

//--------------------------------- Resource Manager --------------------------------//
class ResourceManager {
public:
int availResources;
int usedResources;
int current_cycle;
int availMemoryPorts;
int usedMemoryPorts;

ResourceManager( int n_resources, int memoryports)
	 :availResources(n_resources),availMemoryPorts(memoryports),usedResources(0) {}

 void reset(){
 	usedResources=0;
 	usedMemoryPorts=0;
 }

 bool canSchedule(const Instruction *I){
	
	if(I && (isa<LoadInst>(I) || isa<StoreInst>(I))){
		if(usedMemoryPorts<availMemoryPorts){
			schedule(I);
			return true;
		}
		else return false;
	}
	else {

		if (usedResources<availResources) {
			if(I && !isa<PHINode>(I)) schedule(I);	
			return true;
		}	
		else return false;
	}
 }


 void schedule(const Instruction *I){
	
	if(I && (isa<LoadInst>(I) || isa<StoreInst>(I))){
	 	usedMemoryPorts +=1;
	} else 
		usedResources +=1;
 }

};// end of class ResourceManager
	
		
class ILPEstimator : public BasicBlockPass {
public:
  static char ID;
  ILPEstimator() : BasicBlockPass(ID),rm(ResourceCount,memoryports){}
  ResourceManager rm;
  int BB_latency;
  int ASAPfinishTime;
  std::map<const Instruction*,int>ASAPschedule;
  std::map<const Instruction*,int>ALAPschedule;
  std::set<const Instruction*> readyList;
  std::set<const Instruction*> readyListP;
  std::map<const Instruction*,int> schedule;
  
  //--------------------------------- runOnBasicBlock ------------------------------//
  bool runOnBasicBlock(BasicBlock &BB) override {	  
    errs().write_escaped(BB.getName()) << "\n";
    BB_latency = estimateWCET(BB);
    ASAPfinishTime = scheduleASAP(BB,ASAPschedule);
    scheduleALAP(BB,ALAPschedule,ASAPfinishTime);
    listScheduler(BB,ResourceCount,schedule);
    return false;
  }

  //--------------------------------- estimateWCET ------ ------------------------//
  int estimateWCET(BasicBlock &BB){
    int wcet=0;
    for(auto &I: BB){
  	wcet += getInstructionLatency(&I);
    }
    return wcet;
  }

  //--------------------------------- getInstructionLatency -----------------------//
  int getInstructionLatency(const Instruction *I){
    if(!I) return 0;
    if(isa<LoadInst>(I)) return 2;
    switch(I->getOpcode()){
    case Instruction::Mul:
	return 2;
    default: 
	return 1;
     }
  }

 
  //--------------------------------- PRINT ----- --------------------------------//
  void print(raw_ostream &OS, const Module* = 0) const override {
	//OS << "WCET estimate of the block is:"<<ASAPfinishTime<<"\n";
	//OS << "Schedule:\n";
	//OS << "-----------------------------------------------------\n";
	//for(const auto &entry:ASAPschedule){
	//	const Instruction *i = entry.first;
	//	auto alapIT = ALAPschedule.find(i);
	//	OS << *i <<" |ASAP: " << entry.second << "|ALAP"<<alapIT->second<<"\n";
		
	//}
	OS << "\n";
  }


  //--------------------------------- ASAP SCHEDULE --------------------------------//
  //earliestTime is like: after which you can schedule at any time ----> Lower Bound
  int scheduleASAP(BasicBlock &BB, std::map<const Instruction *, int> &schedule){
      int maxLatency=0;
      int earliestTime=0;
      schedule.clear();
      for(Instruction &inst: BB){
	     //llvm::errs()<<"Users of the instruction:";
	     //inst.dump();
	     int latency = getInstructionLatency(&inst);
	     if(isa<PHINode>(&inst)){
		      	//earliestTime = 0;
			schedule[&inst] = earliestTime;
	      }
	     else {
	      	for(User *user:inst.users()){
			Instruction *I = dyn_cast<Instruction>(user);
			if(I && I->getParent() == &BB && !isa<PHINode>(I)){
				//llvm::errs()<<"Used by :";
				//I->dump();	
				if(schedule.find(I) != schedule.end()){
					earliestTime = std::min(earliestTime,schedule[I]-latency);
			 	}	
			}	//I-> dump();
		}	
	      	schedule[&inst] += earliestTime;
	      	//errs()<<"earliesTime"<<earliestTime<<"\n";
		int instr_completion = schedule[&inst] + latency;
	      	maxLatency = std::max(maxLatency,instr_completion);
	      	for(User *user:inst.users()){
			Instruction *I = dyn_cast<Instruction>(user);
			if(I && I->getParent() == &BB && !isa<PHINode>(I)){
				//if(schedule.find(I) == schedule.end()){
					schedule[I] = std::max(schedule[I],instr_completion);
				//}
			}	//I-> dump();
		}	
	     }

	}
	return maxLatency;
  }

  //--------------------------------- ALAP SCHEDULE --------------------------------//
  //latestTime is like: before which you can schedule at any time ---> Upper Bound
  void scheduleALAP(BasicBlock &BB, std::map<const Instruction *,int>&schedule, int ASAPFinishTime){
	schedule.clear();
	int latestTime = ASAPFinishTime;


	//Phi Node processing 
	if(PHINode *phistart = dyn_cast<PHINode>( &BB.front())){
		schedule[&(*phistart)] = 0;
	}


	if(PHINode *phistart = dyn_cast<PHINode>( &BB.back())){
		schedule[&(*phistart)] = ASAPFinishTime;	
	}

	// For other instructions
	for (auto it=BB.rbegin(),end=BB.rend();it!=end;it++){
		int lat = 0;
		bool instUser=false;
		
	     if(!isa<PHINode>(*it)){ 
		Instruction *inst = &(*it);
		lat = getInstructionLatency(inst);
		for(auto user: inst->users()){
			instUser = true;
			Instruction *userInst = dyn_cast<Instruction>(user);
		      	if(userInst && userInst->getParent() == &BB && !isa<PHINode>(userInst)){
				if(schedule.find(userInst) != schedule.end()){//The instr which we are checking is scheduled, otherwise find will give .end()
					schedule[inst] = std::min(latestTime,std::max(0,schedule[userInst] - lat));
				}		
			}
		}
		if(!instUser){schedule[inst] = std::max(0,latestTime-lat);}
	       }
	}
  }


private:

  //--------------------------------- Instruction Score --------------------------------//
  int instructionScore(const Instruction *I,int instrCount,int BBsize){
	
	  //errs()<<"Instr count" << instrCount<<"\n";
	  if(I->isTerminator()){
	
	  	//errs()<<"check for terminator" << *I <<"\n";
		if(instrCount == BBsize -1){
			return( ASAPfinishTime - ALAPschedule[I]); 
		}else 
			return 0;
	}else{	
	  return(ASAPfinishTime - ALAPschedule[I]); 
	}
   }

  //--------------------------------- selectNextOperation -------------------------------//
  const Instruction *selectNextOperation(std::set<const Instruction *> &readyListP,ResourceManager &rm,int instrCount,int BBsize){
	int score=0;
	const Instruction *I; 
	for(auto it=readyListP.begin();it !=readyListP.end();++it){
		int curInstScore = instructionScore(*it,instrCount,BBsize);
		if(curInstScore >= score){
			I= *it;
			score=curInstScore;
		}
	}
		
	if(I && rm.canSchedule(I)){	
		return I;
	}
	else 
		return NULL;
  }

 //--------------------------------- listScheduler -------------------------------------//
 void listScheduler(BasicBlock &BB,int availResources,std::map<const Instruction*,int> &schedule){
      int current_cycle=0;
      int instrCount=0;
      int BBsize = BB.size();
      errs() << "availResources inside listScheduler"<<availResources <<"\n";
      errs() << "listScheduler for BB: "<< BB.getName() <<"\n";
      ResourceManager rm(availResources,memoryports);
      int totalSize = schedule.size() + BBsize;	
      
      while((schedule.size() != totalSize)){ 	
	  // Preparation of readyList     
	 for(const Instruction &I: BB){
	      if(isa<PHINode>(&I) && ((schedule.find(&I) == schedule.end()) && (readyList.find(&I)==readyList.end())&& (readyListP.find(&I)==readyListP.end()))){
		schedule[&I] = current_cycle;
		++instrCount;
		readyList.insert(&I);
	      }else{
		 if(!isa<PHINode>(&I) && (schedule.find(&I) == schedule.end()) && (readyList.find(&I)==readyList.end())&& (readyListP.find(&I)==readyListP.end())){//not in schedule,readyList,readyListP
			bool allDepsScheduled=true;
		 	 for(auto &operand:I.operands()){
				llvm::Value *val = operand.get();
				if(llvm::Instruction *producer = llvm::dyn_cast<llvm::Instruction>(val)){//if val is other instr
					if(schedule.find(producer) == schedule.end()){//producer is scheduled
						allDepsScheduled = false;
						break;
					}
				}
			}
			
			if(allDepsScheduled){
				readyList.insert(&I);
				//errs() << "readyList has inserted "<< I <<"\n";
			}
		  }
	 	}	 
	  }
	
 	   // Preparation of readyListP     
	  for (auto it=readyList.begin();it!=readyList.end();){
		  bool allDepsReady = true;
	  	  if(*it && isa<PHINode>(*it)){
			readyListP.insert(*it);
			it = readyList.erase(it);
      			//errs() << "DB:LS- I am a phi node, put inside readyListP "<<readyListP.size()<<"\n";
		  	 ++it;
		  } else {
		     for(auto &operand:(*it)->operands()){
			llvm::Value *val = operand.get();
			if(llvm::Instruction *producer = llvm::dyn_cast<llvm::Instruction>(val)){//if val is other instr
				int latency = getInstructionLatency(producer);
				if(schedule.find(producer)!= schedule.end()){//producer is scheduled
	  				if(schedule[producer] + latency > current_cycle){
						allDepsReady = false;
						break;
					
						}
				
					}
				}
		  	}
		  
		  	if(allDepsReady){
				//errs() << "readyListP has inserted and removed from readyList "<< *(*it) <<"\n";
				readyListP.insert(*it);
				it = readyList.erase(it);
		  	}else ++it;
		  }
		      	//errs() << "size of readyListP" <<readyListP.size()<<"\n";
	  }
	
	  // Scheduling from readyListPrime
	  while(!readyListP.empty()){
	   	//errs() << "Inside readyListP at size " <<readyListP.size()<<"\n";
	  	const Instruction* I = selectNextOperation(readyListP,rm,instrCount,BBsize);
	      	
		if(I && isa<PHINode>(I))
			readyListP.erase(I);
		else{
			if(I!=NULL){
		      		//llvm::errs() << "Loop: I: " << I << "at cycle" << current_cycle << "\n";
		      		schedule[I] = current_cycle;
		      		readyListP.erase(I);
		      		++instrCount;
				//errs() << "size of readyListP" <<readyListP.size()<<"\n";
			}
	     		else break; 
	 	} 
	   }
	  //errs() << "current cycle:" <<current_cycle<<"\n";
	  ++current_cycle;
	  rm.reset(); 
      }

	//Display
	for(const auto& [instr,cycle] : schedule){
		llvm::errs() << "Instructions:" << *instr << "at cycle" << cycle << "\n";
	}


 }

}; // end of class ILPEstimator



}  // end of anonymous namespace

char ILPEstimator::ID = 0;
//char ResourceManager::ID = 0;

static RegisterPass<ILPEstimator> X("ilp-estimate", "ILP Estimator Pass",
                                    false /* Only looks at CFG */,
                                    false /* Analysis Pass */);
//static RegisterPass<ResourceManager> Y("resource-manager"," Resource Manager Pass",
//				    false,
//				    false);
