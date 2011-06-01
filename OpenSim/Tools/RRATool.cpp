// RRATool.cpp
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Copyright (c) 2006 Stanford University and Realistic Dynamics, Inc.
// Contributors: Frank C. Anderson, Eran Guendelman, Chand T. John
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject
// to the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE AUTHORS,
// CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
// THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// This software, originally developed by Realistic Dynamics, Inc., was
// transferred to Stanford University on November 1, 2006.
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//=============================================================================
// INCLUDES
//=============================================================================
#include <time.h>
#include "RRATool.h"
#include "AnalyzeTool.h"
#include <OpenSim/Common/XMLDocument.h>
#include <OpenSim/Common/XMLNode.h>
#include <OpenSim/Common/IO.h>
#include <OpenSim/Common/GCVSplineSet.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/Model/BodySet.h>
#include "VectorFunctionForActuators.h"
#include <OpenSim/Simulation/Manager/Manager.h>
#include <OpenSim/Simulation/Control/ControlLinear.h>
#include <OpenSim/Simulation/Control/ControlSet.h>
#include <OpenSim/Simulation/Model/CMCActuatorSubsystem.h>
#include <OpenSim/Analyses/Actuation.h>
#include <OpenSim/Analyses/Kinematics.h>
#include <OpenSim/Analyses/InverseDynamics.h>
#include <OpenSim/Simulation/SimbodyEngine/Joint.h>
#include "ForwardTool.h"
#include <OpenSim/Common/DebugUtilities.h>
#include "CMC.h" 
#include "CMC_TaskSet.h"
#include "ActuatorForceTarget.h"
#include "ActuatorForceTargetFast.h"

using namespace std;
using namespace SimTK;
using namespace OpenSim;


//=============================================================================
// CONSTRUCTOR(S) AND DESTRUCTOR
//=============================================================================
//_____________________________________________________________________________
/**
 * Destructor.
 */
RRATool::~RRATool()
{
}
//_____________________________________________________________________________
/**
 * Default constructor.
 */
RRATool::RRATool() :
	AbstractTool(),
	_desiredPointsFileName(_desiredPointsFileNameProp.getValueStr()),
	_desiredKinematicsFileName(_desiredKinematicsFileNameProp.getValueStr()),
    _taskSetFileName(_taskSetFileNameProp.getValueStr()),
	_constraintsFileName(_constraintsFileNameProp.getValueStr()),
	_rraControlsFileName(_rraControlsFileNameProp.getValueStr()),
	_lowpassCutoffFrequency(_lowpassCutoffFrequencyProp.getValueDbl()),
	_optimizerAlgorithm(_optimizerAlgorithmProp.getValueStr()),
	_optimizerDX(_optimizerDXProp.getValueDbl()),
	_convergenceCriterion(_convergenceCriterionProp.getValueDbl()),
	_adjustCOMToReduceResiduals(_adjustCOMToReduceResidualsProp.getValueBool()),
	_initialTimeForCOMAdjustment(_initialTimeForCOMAdjustmentProp.getValueDbl()),
	_finalTimeForCOMAdjustment(_finalTimeForCOMAdjustmentProp.getValueDbl()),
	_adjustedCOMBody(_adjustedCOMBodyProp.getValueStr()),
	_outputModelFile(_outputModelFileProp.getValueStr()),
	_verbose(_verboseProp.getValueBool())
{
	setType("RRATool");
	setNull();
}
//_____________________________________________________________________________
/**
 * Construct from an XML property file.
 *
 * @param aFileName File name of the XML document.
 */
RRATool::RRATool(const string &aFileName, bool aLoadModel) :
	AbstractTool(aFileName, false),
	_desiredPointsFileName(_desiredPointsFileNameProp.getValueStr()),
	_desiredKinematicsFileName(_desiredKinematicsFileNameProp.getValueStr()),
	//_externalLoadsFileName(_externalLoadsFileNameProp.getValueStr()),
	//_externalLoadsModelKinematicsFileName(_externalLoadsModelKinematicsFileNameProp.getValueStr()),
    _taskSetFileName(_taskSetFileNameProp.getValueStr()),
	_constraintsFileName(_constraintsFileNameProp.getValueStr()),
	_rraControlsFileName(_rraControlsFileNameProp.getValueStr()),
	_lowpassCutoffFrequency(_lowpassCutoffFrequencyProp.getValueDbl()),
	_optimizerAlgorithm(_optimizerAlgorithmProp.getValueStr()),
	_optimizerDX(_optimizerDXProp.getValueDbl()),
	_convergenceCriterion(_convergenceCriterionProp.getValueDbl()),
	_adjustCOMToReduceResiduals(_adjustCOMToReduceResidualsProp.getValueBool()),
	_initialTimeForCOMAdjustment(_initialTimeForCOMAdjustmentProp.getValueDbl()),
	_finalTimeForCOMAdjustment(_finalTimeForCOMAdjustmentProp.getValueDbl()),
	_adjustedCOMBody(_adjustedCOMBodyProp.getValueStr()),
	_outputModelFile(_outputModelFileProp.getValueStr()),
	_verbose(_verboseProp.getValueBool())
{
	setType("RRATool");
	setNull();
	updateFromXMLNode();
	if(aLoadModel){
		loadModel(aFileName, &_originalForceSet);
		// Append to or replace model forces with those (i.e. actuators) specified by the analysis
		updateModelForces(*_model, aFileName);
		setModel(*_model);	
		setToolOwnsModel(true);
	}
}

//_____________________________________________________________________________
/**
 * Copy constructor.
 *
 * Copy constructors for all Tools do not copy the Object's DOMnode
 * and XMLDocument.  The reason for this is that for the
 * object and all its derived classes to establish the correct connection
 * to the XML document nodes, the the object would need to reconstruct based
 * on the XML document not the values of the object's member variables.
 *
 * There are three proper ways to generate an XML document for a Tool:
 *
 * 1) Construction based on XML file (@see Tool(const char *aFileName)).
 * In this case, the XML document is created by parsing the XML file.
 *
 * 2) Construction by Tool(const XMLDocument *aDocument).
 * This constructor explictly requests construction based on an
 * XML document that is held in memory.  In this way the proper connection
 * between an object's node and the corresponding node within the XML
 * document is established.
 * This constructor is a copy constructor of sorts because all essential
 * Tool member variables should be held within the XML document.
 * The advantage of this style of construction is that nodes
 * within the XML document, such as comments that may not have any
 * associated Tool member variable, are preserved.
 *
 * 3) A call to generateXMLDocument().
 * This method generates an XML document for the Tool from scratch.
 * Only the essential document nodes are created (that is, nodes that
 * correspond directly to member variables.).
 *
 * @param aTool Object to be copied.
 * @see Tool(const XMLDocument *aDocument)
 * @see Tool(const char *aFileName)
 * @see generateXMLDocument()
 */
RRATool::
RRATool(const RRATool &aTool) :
	AbstractTool(aTool),
	_desiredPointsFileName(_desiredPointsFileNameProp.getValueStr()),
	_desiredKinematicsFileName(_desiredKinematicsFileNameProp.getValueStr()),
    _taskSetFileName(_taskSetFileNameProp.getValueStr()),
	_constraintsFileName(_constraintsFileNameProp.getValueStr()),
	_rraControlsFileName(_rraControlsFileNameProp.getValueStr()),
	_lowpassCutoffFrequency(_lowpassCutoffFrequencyProp.getValueDbl()),
	_optimizerAlgorithm(_optimizerAlgorithmProp.getValueStr()),
	_optimizerDX(_optimizerDXProp.getValueDbl()),
	_convergenceCriterion(_convergenceCriterionProp.getValueDbl()),
	_adjustCOMToReduceResiduals(_adjustCOMToReduceResidualsProp.getValueBool()),
	_initialTimeForCOMAdjustment(_initialTimeForCOMAdjustmentProp.getValueDbl()),
	_finalTimeForCOMAdjustment(_finalTimeForCOMAdjustmentProp.getValueDbl()),
	_adjustedCOMBody(_adjustedCOMBodyProp.getValueStr()),
	_outputModelFile(_outputModelFileProp.getValueStr()),
	_verbose(_verboseProp.getValueBool())
{
	setType("RRATool");
	setNull();
	*this = aTool;
}

//_____________________________________________________________________________
/**
 * Virtual copy constructor.
 *
 * @return Copy of this object.
 */
Object* RRATool::
copy() const
{
	RRATool *object = new RRATool(*this);
	return(object);
}

//_____________________________________________________________________________
/**
 * Set all member variables to their null or default values.
 */
void RRATool::
setNull()
{
	setupProperties();

	_desiredPointsFileName = "";
	_desiredKinematicsFileName = "";
    _taskSetFileName = "";
	_constraintsFileName = "";
	_rraControlsFileName = "";
	_lowpassCutoffFrequency = -1.0;
	//_lowpassCutoffFrequencyForLoadKinematics = -1.0;
 	_optimizerAlgorithm = "ipopt";
	_optimizerDX = 1.0e-4;
	_convergenceCriterion = 1.0e-6;
	_adjustedCOMBody = "";
	_adjustCOMToReduceResiduals = false;
	_initialTimeForCOMAdjustment = -1;
	_finalTimeForCOMAdjustment = -1;
	_outputModelFile = "";
	_adjustKinematicsToReduceResiduals=true;
	_verbose = false;
	_targetDT = .001;
    _replaceForceSet = false;   // default should be false for Forward.


}
//_____________________________________________________________________________
/**
 * Give this object's properties their XML names and add them to the property
 * list held in class Object (@see OpenSim::Object).
 */
void RRATool::setupProperties()
{
	string comment;

	comment = "Motion (.mot) or storage (.sto) file containing the desired point trajectories.";
	_desiredPointsFileNameProp.setComment(comment);
	_desiredPointsFileNameProp.setName("desired_points_file");
	_propertySet.append( &_desiredPointsFileNameProp );

	comment = "Motion (.mot) or storage (.sto) file containing the desired kinematic trajectories.";
	_desiredKinematicsFileNameProp.setComment(comment);
	_desiredKinematicsFileNameProp.setName("desired_kinematics_file");
	_propertySet.append( &_desiredKinematicsFileNameProp );

	comment = "File containing the tracking tasks. Which coordinates are tracked and with what weights are specified here.";  	 	 
    _taskSetFileNameProp.setComment(comment); 		 
    _taskSetFileNameProp.setName("task_set_file"); 		 
    _propertySet.append( &_taskSetFileNameProp );

	comment = "File containing the constraints on the controls.";
	_constraintsFileNameProp.setComment(comment);
	_constraintsFileNameProp.setName("constraints_file");
	_propertySet.append( &_constraintsFileNameProp );

	comment = "File containing the controls output by RRA.";
	comment += " These can be used to place constraints on the residuals during CMC.";
	_rraControlsFileNameProp.setComment(comment);
	_rraControlsFileNameProp.setName("rra_controls_file");
	_propertySet.append( &_rraControlsFileNameProp );

	comment = "Low-pass cut-off frequency for filtering the desired kinematics.";
	comment += " A negative value results in no filtering. The default value is -1.0, so no filtering.";
	_lowpassCutoffFrequencyProp.setComment(comment);
	_lowpassCutoffFrequencyProp.setName("lowpass_cutoff_frequency");
	_propertySet.append( &_lowpassCutoffFrequencyProp );


	comment = "Preferred optimizer algorithm (currently support \"ipopt\" or \"cfsqp\", "
				 "the latter requiring the osimFSQP library.";
	_optimizerAlgorithmProp.setComment(comment);
	_optimizerAlgorithmProp.setName("optimizer_algorithm");
	_propertySet.append( &_optimizerAlgorithmProp );

	comment = "Perturbation size used by the optimizer to compute numerical derivatives. "
				 "A value between 1.0e-4 and 1.0e-8 is usually approprieate.";
	_optimizerDXProp.setComment(comment);
	_optimizerDXProp.setName("optimizer_derivative_dx");
	_propertySet.append( &_optimizerDXProp );

	comment = "Convergence criterion for the optimizer. The smaller this value, the deeper the convergence. "
				 "Decreasing this number can improve a solution, but will also likely increase computation time.";
	_convergenceCriterionProp.setComment(comment);
	_convergenceCriterionProp.setName("optimizer_convergence_criterion");
	_propertySet.append( &_convergenceCriterionProp );

	comment = "Flag (true or false) indicating whether or not to make an adjustment "
				 "in the center of mass of a body to reduced DC offsets in MX and MZ. "
				 "If true, a new model is writen out that has altered anthropometry.";
	_adjustCOMToReduceResidualsProp.setComment(comment);
	_adjustCOMToReduceResidualsProp.setName("adjust_com_to_reduce_residuals");
	_propertySet.append( &_adjustCOMToReduceResidualsProp );

	comment = "Initial time used when computing average residuals in order to adjust "
				 "the body's center of mass.  If both initial and final time are set to "
				 "-1 (their default value) then the main initial and final time settings will be used.";
	_initialTimeForCOMAdjustmentProp.setComment(comment);
	_initialTimeForCOMAdjustmentProp.setName("initial_time_for_com_adjustment");
	_propertySet.append( &_initialTimeForCOMAdjustmentProp );

	comment = "Final time used when computing average residuals in order to adjust "
				 "the body's center of mass.";
	_finalTimeForCOMAdjustmentProp.setComment(comment);
	_finalTimeForCOMAdjustmentProp.setName("final_time_for_com_adjustment");
	_propertySet.append( &_finalTimeForCOMAdjustmentProp );

	comment = "Name of the body whose center of mass is adjusted. "
				 "The heaviest segment in the model should normally be chosen. "
				 "For a gait model, the torso segment is usually the best choice.";
	_adjustedCOMBodyProp.setComment(comment);
	_adjustedCOMBodyProp.setName("adjusted_com_body");
	_propertySet.append( &_adjustedCOMBodyProp );

	comment = "Name of the output model file (.osim) containing adjustments to anthropometry "
				 "made to reduce average residuals. This file is written if the property "
				 "adjust_com_to_reduce_residuals is set to true. If a name is not specified, "
				 "the model is written out to a file called adjusted_model.osim.";
	_outputModelFileProp.setComment(comment);
	_outputModelFileProp.setName("output_model_file");
	_propertySet.append( &_outputModelFileProp );

	comment = "True-false flag indicating whether or not to turn on verbose printing for cmc.";
	_verboseProp.setComment(comment);
	_verboseProp.setName("use_verbose_printing");
	_propertySet.append( &_verboseProp );

}


//=============================================================================
// OPERATORS
//=============================================================================
//_____________________________________________________________________________
/**
 * Assignment operator.
 *
 * @return Reference to this object.
 */
RRATool& RRATool::
operator=(const RRATool &aTool)
{
	// BASE CLASS
	AbstractTool::operator=(aTool);

	// MEMEBER VARIABLES
	_desiredPointsFileName = aTool._desiredPointsFileName;
	_desiredKinematicsFileName = aTool._desiredKinematicsFileName;
    _taskSetFileName = aTool._taskSetFileName;
	_constraintsFileName = aTool._constraintsFileName;
	_rraControlsFileName = aTool._rraControlsFileName;
	_lowpassCutoffFrequency = aTool._lowpassCutoffFrequency;
    _targetDT = aTool._targetDT;  	 	 
	_optimizerDX = aTool._optimizerDX;
	_convergenceCriterion = aTool._convergenceCriterion;
	_optimizerAlgorithm = aTool._optimizerAlgorithm;
	_adjustedCOMBody = aTool._adjustedCOMBody;
	_outputModelFile = aTool._outputModelFile;
	_adjustCOMToReduceResiduals = aTool._adjustCOMToReduceResiduals;
	_initialTimeForCOMAdjustment = aTool._initialTimeForCOMAdjustment;
	_finalTimeForCOMAdjustment = aTool._finalTimeForCOMAdjustment;
	_verbose = aTool._verbose;

	return(*this);
}

//=============================================================================
// GET AND SET
//=============================================================================


//=============================================================================
// RUN
//=============================================================================
//_____________________________________________________________________________
/**
 * Run the investigation.
 */
bool RRATool::run()
{
	cout<<"Running tool "<<getName()<<".\n";

	// CHECK FOR A MODEL
	if(_model==NULL) {
		string msg = "ERROR- A model has not been set.";
		cout<<endl<<msg<<endl;
		throw(Exception(msg,__FILE__,__LINE__));
	}
	// OUTPUT DIRECTORY
	// Do the maneuver to change then restore working directory 
	// so that the parsing code behaves prope()rly if called from a different directory
	string saveWorkingDirectory = IO::getCwd();
	string directoryOfSetupFile = IO::getParentDirectory(getDocumentFileName());
	IO::chDir(directoryOfSetupFile);

	try {

	// SET OUTPUT PRECISION
	IO::SetPrecision(_outputPrecision);


	// CHECK PROPERTIES FOR ERRORS/INCONSISTENCIES
	if(_adjustCOMToReduceResiduals) {
		if(_adjustedCOMBody == "")
			throw Exception("RRATool: ERROR- "+_adjustCOMToReduceResidualsProp.getName()+" set to true but "+
								 _adjustedCOMBodyProp.getName()+" not set",__FILE__,__LINE__);
        else if (_model->getBodySet().getIndex(_adjustedCOMBody) == -1)
		    throw Exception("RRATool: ERROR- Body '"+_adjustedCOMBody+"' specified in "+
							     _adjustedCOMBodyProp.getName()+" not found",__FILE__,__LINE__);
	}

    bool externalLoads = createExternalLoads(_externalLoadsFileName, *_model);

    CMC_TaskSet taskSet(_taskSetFileName);  	 	 
    cout<<"\n\n taskSet size = "<<taskSet.getSize()<<endl<<endl; 		 

    CMC* controller = new CMC(_model,&taskSet);	// Need to make it a pointer since Model takes ownership 
    controller->setName( "CMC" );
	controller->setActuators(_model->updActuators());
    _model->addController(controller );
    controller->setIsEnabled(true);
    controller->setUseCurvatureFilter(false);
    controller->setTargetDT(.001);
    controller->setCheckTargetTime(true);

	//Make sure system is uptodate with model (i.e. added actuators, etc...)
	SimTK::State s = _model->initSystem();
    _model->getMultibodySystem().realize(s, Stage::Position );
     taskSet.setModel(*_model);
    _model->equilibrateMuscles(s);
  
	// ---- INPUT ----
	// DESIRED POINTS AND KINEMATICS
	if(_desiredPointsFileName=="" && _desiredKinematicsFileName=="") {
		cout<<"ERROR- a desired points file and desired kinematics file were not specified.\n\n";
		IO::chDir(saveWorkingDirectory);
		return false;
	}

	Storage *desiredPointsStore=NULL;
	bool desiredPointsFlag = false;
	if(_desiredPointsFileName=="") {
		cout<<"\n\nWARN- a desired points file was not specified.\n\n";
	} else {
		cout<<"\n\nLoading desired points from file "<<_desiredPointsFileName<<" ...\n";
		desiredPointsStore = new Storage(_desiredPointsFileName);
		desiredPointsFlag = true;
	}

	Storage *desiredKinStore=NULL;
	bool desiredKinFlag = false;
	if(_desiredKinematicsFileName=="") {
		cout<<"\n\nWARN- a desired kinematics file was not specified.\n\n";
	} else {
		cout<<"\n\nLoading desired kinematics from file "<<_desiredKinematicsFileName<<" ...\n";
        desiredKinStore = new Storage();
        loadQStorage( _desiredKinematicsFileName, *desiredKinStore );
		desiredKinFlag = true;
	}

	// ---- INITIAL AND FINAL TIME ----
	// NOTE: important to do this before padding (for filtering)
	// Initial Time
	if(desiredPointsFlag) {
		double ti = desiredPointsStore->getFirstTime();
		if(_ti<ti) {
			cout<<"\nThe initial time set for the cmc run precedes the first time\n";
			cout<<"in the desired points file "<<_desiredPointsFileName<<".\n";
			cout<<"Resetting the initial time from "<<_ti<<" to "<<ti<<".\n\n";
			_ti = ti;
		}
		// Final time
		double tf = desiredPointsStore->getLastTime();
		if(_tf>tf) {
			cout<<"\n\nWARN- The final time set for the cmc run is past the last time stamp\n";
			cout<<"in the desired points file "<<_desiredPointsFileName<<".\n";
			cout<<"Resetting the final time from "<<_tf<<" to "<<tf<<".\n\n";
			_tf = tf;
		}
	}

	// Initial Time
	if(desiredKinFlag) {
		double ti = desiredKinStore->getFirstTime();
		if(_ti<ti) {
			cout<<"\nThe initial time set for the cmc run precedes the first time\n";
			cout<<"in the desired kinematics file "<<_desiredKinematicsFileName<<".\n";
			cout<<"Resetting the initial time from "<<_ti<<" to "<<ti<<".\n\n";
			_ti = ti;
		}
		// Final time
		double tf = desiredKinStore->getLastTime();
		if(_tf>tf) {
			cout<<"\n\nWARN- The final time set for the cmc run is past the last time stamp\n";
			cout<<"in the desired kinematics file "<<_desiredKinematicsFileName<<".\n";
			cout<<"Resetting the final time from "<<_tf<<" to "<<tf<<".\n\n";
			_tf = tf;
		}
	}

	// Filter
	// Eran: important to filter *before* calling formCompleteStorages because we need the
	// constrained coordinates (e.g. tibia-patella joint angle) to be consistent with the
	// filtered trajectories
	if(desiredPointsFlag) {
		desiredPointsStore->pad(60);
		desiredPointsStore->print("desiredPoints_padded.sto");
		if(_lowpassCutoffFrequency>=0) {
			int order = 50;
			cout<<"\n\nLow-pass filtering desired points with a cutoff frequency of ";
			cout<<_lowpassCutoffFrequency<<"...";
			desiredPointsStore->lowpassFIR(order,_lowpassCutoffFrequency);
		} else {
			cout<<"\n\nNote- not filtering the desired points.\n\n";
		}
	}

	if(desiredKinFlag) {
		desiredKinStore->pad(60);
		desiredKinStore->print("desiredKinematics_padded.sto");
		if(_lowpassCutoffFrequency>=0) {
			int order = 50;
			cout<<"\n\nLow-pass filtering desired kinematics with a cutoff frequency of ";
			cout<<_lowpassCutoffFrequency<<"...\n\n";
			desiredKinStore->lowpassFIR(order,_lowpassCutoffFrequency);
		} else {
			cout<<"\n\nNote- not filtering the desired kinematics.\n\n";
		}
	}

     // TASK SET
    if(_taskSetFileName=="") {  	 	 
        cout<<"ERROR- a task set was not specified\n\n"; 		 
        IO::chDir(saveWorkingDirectory); 		 
        return false; 		 
    }

	_model->printDetailedInfo(s, cout);

	int nq = _model->getNumCoordinates();
	int nu = _model->getNumSpeeds();
	int na = _model->getActuators().getSize();

	// Form complete storage objects for the q's and u's
	// This means filling in unspecified generalized coordinates and
	// setting constrained coordinates to their valid values.
	Storage *qStore=NULL;
	Storage *uStore=NULL;

	if(desiredKinFlag) {
        _model->getMultibodySystem().realize(s, Stage::Time );
	    _model->getSimbodyEngine().formCompleteStorages(s, *desiredKinStore,qStore,uStore);
		_model->getSimbodyEngine().convertDegreesToRadians(*qStore);
		_model->getSimbodyEngine().convertDegreesToRadians(*uStore);
	}

	// GROUND REACTION FORCES
    if( externalLoads ) {
	   initializeExternalLoads(s, _ti, _tf);
    }

	// Adjust COM to reduce residuals (formerly RRA pass 1) if requested
	if(desiredKinFlag) {
		if(_adjustCOMToReduceResiduals) {
		
			adjustCOMToReduceResiduals(s, *qStore,*uStore);

			// If not adjusting kinematics, we don't proceed with CMC, and just stop here.
			if(!_adjustKinematicsToReduceResiduals) {
				cout << "No kinematics adjustment requested." << endl;
				delete qStore;
				delete uStore;
				writeAdjustedModel();
				IO::chDir(saveWorkingDirectory);
				return true;
            }
		}
	}

	// Spline
	GCVSplineSet *posSet=NULL;
	if(desiredPointsFlag) {
		cout<<"\nConstructing function set for tracking desired points...\n\n";
		posSet = new GCVSplineSet(5,desiredPointsStore);

		Storage *velStore=posSet->constructStorage(1);
		GCVSplineSet velSet(5,velStore);
		delete velStore; velStore=NULL;

		// Print acc for debugging
		Storage *accStore=posSet->constructStorage(2);
		accStore->print("desiredPoints_splinefit_accelerations.sto");
		delete accStore; accStore=NULL;	
	}

	GCVSplineSet *qSet=NULL;
	GCVSplineSet *uSet=NULL;
	if(desiredKinFlag) {
		cout<<"\nConstructing function set for tracking desired kinematics...\n\n";
		qSet = new GCVSplineSet(5,qStore);
		delete qStore; qStore = NULL;

		delete uStore;
		uStore = qSet->constructStorage(1);
		uSet = new GCVSplineSet(5,uStore);
		delete uStore; uStore=NULL;

		// Print dudt for debugging
		Storage *dudtStore = qSet->constructStorage(2);
		dudtStore->print("desiredKinematics_splinefit_accelerations.sto");
		delete dudtStore; dudtStore=NULL;
	}

	// ANALYSES
	addNecessaryAnalyses();

	GCVSplineSet *qAndPosSet=NULL;
	qAndPosSet = new GCVSplineSet();
	if(desiredPointsFlag) {
		int nps=posSet->getSize();
		for(int i=0;i<nps;i++) {
			qAndPosSet->append(&posSet->get(i));
		}
	}
	if(desiredKinFlag) {
		int nqs=qSet->getSize();
		for(int i=0;i<nqs;i++) {
			qAndPosSet->append(&qSet->get(i));
		}
	}
	if (taskSet.getDataFileName()!=""){
		// Add functions from data files to track states
		Storage stateStorage(taskSet.getDataFileName());
		GCVSplineSet* stateFuntcions = new GCVSplineSet(3, &stateStorage);
		for (int i=0; i< stateFuntcions->getSize(); i++)
			qAndPosSet->append(stateFuntcions->get(i));

	}
    taskSet.setFunctions(*qAndPosSet);  	 	 
 

	// CONSTRAINTS ON THE CONTROLS
	ControlSet *controlConstraints = NULL;
	if(_constraintsFileName!="") {
		controlConstraints = new ControlSet(_constraintsFileName);
	}

	// RRA CONTROLS
	ControlSet *rraControlSet = constructRRAControlSet(controlConstraints);

	// ---- INITIAL STATES ----
	Array<double> q(0.0,nq);
	Array<double> u(0.0,nu);
	if(desiredKinFlag) {
		cout<<"Using the generalized coordinates specified in "<<_desiredKinematicsFileName;
		cout<<" to set the initial configuration.\n" << endl;
		qSet->evaluate(q,0,_ti);
		uSet->evaluate(u,0,_ti);
	} else {
		cout<<"Using the generalized coordinates specified as zeros ";
		cout<<" to set the initial configuration.\n" << endl;
	}

	for(int i=0;i<nq;i++) s.updQ()[i] = q[i];
	for(int i=0;i<nu;i++) s.updU()[i] = u[i];

	// Actuator force predictor
	// This requires the trajectories of the generalized coordinates
	// to be specified.
	
	string rraControlName;
    CMCActuatorSystem actuatorSystem;
    CMCActuatorSubsystem cmcActSubsystem(actuatorSystem, _model);
	cmcActSubsystem.setCoordinateTrajectories(qSet);
    actuatorSystem.realizeTopology();
	// initialize the actuator states 
	SimTK::State& actuatorSystemState = actuatorSystem.updDefaultState(); 
    
	SimTK::Vector &actSysZ = actuatorSystemState.updZ();
	const SimTK::Vector &modelZ = _model->getForceSubsystem().getZ(s);
	
	int nra = actSysZ.size();
	int nrm = modelZ.size();

	assert(nra == nrm);
	actSysZ = modelZ;

	VectorFunctionForActuators *predictor =
		new VectorFunctionForActuators(&actuatorSystem, _model, &cmcActSubsystem);

	controller->setActuatorForcePredictor(predictor);
	controller->updTaskSet().setFunctions(*qAndPosSet);

	// Optimization target
	OptimizationTarget *target = NULL;
	if(false) {
		target = new ActuatorForceTargetFast(s, na,controller);
	} else {
		target = new ActuatorForceTarget(na,controller);
	}
	target->setDX(_optimizerDX);

	// Pick optimizer algorithm
	SimTK::OptimizerAlgorithm algorithm = SimTK::InteriorPoint;
	if(IO::Uppercase(_optimizerAlgorithm) == "CFSQP") {
		if(!SimTK::Optimizer::isAlgorithmAvailable(SimTK::CFSQP)) {
			std::cout << "CFSQP optimizer algorithm unavailable.  Will try to use IPOPT instead." << std::endl;
			algorithm = SimTK::InteriorPoint;
		} else {
			std::cout << "Using CFSQP optimizer algorithm." << std::endl;
			algorithm = SimTK::CFSQP;
		}
	} else if(IO::Uppercase(_optimizerAlgorithm) == "IPOPT") {
		std::cout << "Using IPOPT optimizer algorithm." << std::endl;
		algorithm = SimTK::InteriorPoint;
	} else {
		throw Exception("RRATool: ERROR- Unrecognized optimizer algorithm: '"+_optimizerAlgorithm+"'",__FILE__,__LINE__);
	}

	SimTK::Optimizer *optimizer = new SimTK::Optimizer(*target, algorithm);
	controller->setOptimizationTarget(target, optimizer);

	cout<<"\nSetting optimizer print level to "<< (_verbose?4:0) <<".\n";
	optimizer->setDiagnosticsLevel(_verbose?4:0);
	cout<<"Setting optimizer convergence criterion to "<<_convergenceCriterion<<".\n";
	optimizer->setConvergenceTolerance(_convergenceCriterion);
	cout<<"Setting optimizer maximum iterations to "<<2000<<".\n";
	optimizer->setMaxIterations(2000);
	optimizer->useNumericalGradient(false); // Use our own central difference approximations
	optimizer->useNumericalJacobian(false);
	if(algorithm == SimTK::InteriorPoint) {
		// Some IPOPT-specific settings
		optimizer->setLimitedMemoryHistory(500); // works well for our small systems
		optimizer->setAdvancedBoolOption("warm_start",true);
		optimizer->setAdvancedRealOption("obj_scaling_factor",1);
		optimizer->setAdvancedRealOption("nlp_scaling_max_gradient",100);
	}

	if(_verbose) cout<<"\nSetting cmc controller to use verbose printing."<<endl;
	else cout<<"\nSetting cmc controller to not use verbose printing."<<endl;
	controller->setUseVerbosePrinting(_verbose);

	controller->setCheckTargetTime(true);

	// ---- SIMULATION ----
	//
	// Manager
    RungeKuttaMersonIntegrator integrator(_model->getMultibodySystem());
	integrator.setMaximumStepSize(_maxDT);
	integrator.setMinimumStepSize(_minDT);
	integrator.setAccuracy(_errorTolerance);
    Manager manager(*_model, integrator);
	
	_model->setAllControllersEnabled( true );

	manager.setSessionName(getName());
	manager.setInitialTime(_ti);
	manager.setFinalTime(_tf-_targetDT-SimTK::Zero);

	// Initialize integrand controls using controls read in from file (which specify min/max control values)
	initializeControlSetUsingConstraints(rraControlSet,controlConstraints, controller->updControlSet());

	// Initial auxilliary states
	time_t startTime,finishTime;
	struct tm *localTime;
	double elapsedTime;
	if( s.getNZ() > 0) { // If there are actuator states (i.e. muscles dynamics)
		cout<<"\n\n\n";
		cout<<"================================================================\n";
		cout<<"================================================================\n";
		cout<<"Computing initial values for muscles states (activation, length)\n";
		time(&startTime);
		localTime = localtime(&startTime);
		cout<<"Start time = "<<asctime(localTime);
		cout<<"\n================================================================\n";
		controller->computeInitialStates(s,_ti);
		time(&finishTime);
		cout<<endl;
        // copy the final states from the last integration 
        s.updY() = cmcActSubsystem.getCompleteState().getY();
		cout<<"-----------------------------------------------------------------\n";
		cout<<"Finished computing initial states:\n";
		cout<<"-----------------------------------------------------------------\n";
		cout<<"=================================================================\n";
		localTime = localtime(&startTime);
		cout<<"Start time   = "<<asctime(localTime);
		localTime = localtime(&finishTime);
		cout<<"Finish time  = "<<asctime(localTime);
		elapsedTime = difftime(finishTime,startTime);
		cout<<"Elapsed time = "<<elapsedTime<<" seconds.\n";
		cout<<"=================================================================\n";
	} else {
        cmcActSubsystem.setCompleteState( s );
	    actuatorSystemState.updTime() = _ti; 
        s.updTime() = _ti;
        actuatorSystem.realize(actuatorSystemState, Stage::Time );
        controller->setTargetDT(1.0e-8);
		controller->computeControls( s, controller->updControlSet() );
        controller->setTargetDT(_targetDT);
    }
    manager.setInitialTime(_ti);

	// ---- INTEGRATE ----
	cout<<"\n\n\n";
	cout<<"================================================================\n";
	cout<<"================================================================\n";
	cout<<"Using CMC to track the specified kinematics\n";
	cout<<"Integrating from "<<_ti<<" to "<<_tf<<endl;
    s.updTime() = _ti;
	controller->setTargetTime( _ti );
	time(&startTime);
	localTime = localtime(&startTime);
	cout<<"Start time = "<<asctime(localTime);
	cout<<"================================================================\n";

    _model->getMultibodySystem().realize(s, Stage::Acceleration );

	controller->updTaskSet().computeAccelerations(s);

    cmcActSubsystem.setCompleteState( s );

	// Set output file names so that files are flushed regularly in case we fail
	IO::makeDir(getResultsDir());	// Create directory for output in case it doesn't exist
	manager.getStateStorage().setOutputFileName(getResultsDir() + "/" + getName() + "_states.sto");
	try {
		manager.integrate(s);
	}
	catch(Exception &x) {
		// TODO: eventually might want to allow writing of partial results
		x.print(cout);
		IO::chDir(saveWorkingDirectory);
		// close open files if we die prematurely (e.g. Opt fail)
		manager.getStateStorage().print(getResultsDir() + "/" + getName() + "_states.sto");
		return false;
	}
	time(&finishTime);
	cout<<"----------------------------------------------------------------\n";
	cout<<"Finished tracking the specified kinematics\n";
    if( _verbose ){
      std::cout << "states= " << s.getY() << std::endl;
    }
	cout<<"=================================================================\n";
	localTime = localtime(&startTime);
	cout<<"Start time   = "<<asctime(localTime);
	localTime = localtime(&finishTime);
	cout<<"Finish time  = "<<asctime(localTime);
	elapsedTime = difftime(finishTime,startTime);
	cout<<"Elapsed time = "<<elapsedTime<<" seconds.\n";
	cout<<"================================================================\n\n\n";

	// ---- RESULTS -----
	double dt = 0.001;
	printResults(getName(),getResultsDir(),dt); // this will create results directory if necessary
    controller->updControlSet().print(getResultsDir() + "/" + getName() + "_controls.xml");
	_model->printControlStorage(getResultsDir() + "/" + getName() + "_controls.sto");
	manager.getStateStorage().print(getResultsDir() + "/" + getName() + "_states.sto");

	Storage statesDegrees(manager.getStateStorage());
	_model->getSimbodyEngine().convertRadiansToDegrees(statesDegrees);
	statesDegrees.setWriteSIMMHeader(true);
	statesDegrees.print(getResultsDir() + "/" + getName() + "_states_degrees.mot");

	controller->getPositionErrorStorage()->print(getResultsDir() + "/" + getName() + "_pErr.sto");

    if(_model->getAnalysisSet().getIndex("Actuation") != -1) {
	    Actuation& actuation = (Actuation&)_model->getAnalysisSet().get("Actuation");
	    Array<double> FAve(0.0,3),MAve(0.0,3);
	    Storage *forceStore = actuation.getForceStorage();
	    computeAverageResiduals(*forceStore,FAve,MAve);
	    cout<<"\n\nAverage residuals:\n";
	    cout<<"FX="<<FAve[0]<<" FY="<<FAve[1]<<" FZ="<<FAve[2]<<endl;
	    cout<<"MX="<<MAve[0]<<" MY="<<MAve[1]<<" MZ="<<MAve[2]<<endl<<endl<<endl;

	    // Write the average residuals (DC offsets) out to a file
	    ofstream residualFile((getResultsDir() + "/" + getName() + "_avgResiduals.txt").c_str());
	    residualFile << "Average Residuals:\n\n";
	    residualFile << "FX average = " << FAve[0] << "\n";
	    residualFile << "FY average = " << FAve[1] << "\n";
	    residualFile << "FZ average = " << FAve[2] << "\n";
	    residualFile << "MX average = " << MAve[0] << "\n";
	    residualFile << "MY average = " << MAve[1] << "\n";
	    residualFile << "MZ average = " << MAve[2] << "\n";
	    residualFile.close();
    }

	// Write new model file
	if(_adjustCOMToReduceResiduals) writeAdjustedModel();

	//_model->removeController(controller); // So that if this model is from GUI it doesn't double-delete it.

	} catch(Exception &x) {
		// TODO: eventually might want to allow writing of partial results
		x.print(cout);
		IO::chDir(saveWorkingDirectory);
		// close open files if we die prematurely (e.g. Opt fail)
		
		return false;
	}

	IO::chDir(saveWorkingDirectory);

	return true;
}


//=============================================================================
// UTILITY
//=============================================================================
void RRATool::
writeAdjustedModel() 
{
	if(_outputModelFile=="") {
		cerr<<"Warning: A name for the output model was not set.\n";
		cerr<<"Specify a value for the property "<<_outputModelFileProp.getName();
		cerr<<" in the setup file.\n";
		cerr<<"Writing to adjusted_model.osim ...\n\n";
		_outputModelFile = "adjusted_model.osim";
	}

	// Set the model's actuator set back to the original set.  e.g. in RRA1
	// we load the model but replace its (muscle) actuators with torque actuators.
	// So we need to put back the muscles before writing out the adjusted model.
	// NOTE: use operator= so actuator groups are properly copied over
	_model->updForceSet() = _originalForceSet;

	// CMC was added as a model controller, now remove before printing out
	int c = _model->updControllerSet().getIndex("CMC");
	_model->updControllerSet().remove(c);

	_model->print(_outputModelFile);
}
//_____________________________________________________________________________
/**
 * Compute the average residuals.
 *
 * @param rFAve The computed average force residuals.  The size of rFAve is
 * set to 3.
 * @param rMAve The computed average moment residuals.  The size of rMAve is
 * set to 3.
 */
void RRATool::
computeAverageResiduals(const Storage &aForceStore,OpenSim::Array<double> &rFAve,OpenSim::Array<double> &rMAve)
{

    int iFX, iFY, iFZ, iMX, iMY, iMZ;
	// COMPUTE AVERAGE
	int size = aForceStore.getSmallestNumberOfStates();
	Array<double> ave(0.0);
	ave.setSize(size);
	aForceStore.computeAverage(size,&ave[0]);
  

	// GET INDICES
	  iFX = aForceStore.getStateIndex("FX");
      iFY = aForceStore.getStateIndex("FY");
	  iFZ = aForceStore.getStateIndex("FZ");
	  iMX = aForceStore.getStateIndex("MX");
	  iMY = aForceStore.getStateIndex("MY");
	  iMZ = aForceStore.getStateIndex("MZ");

//cout << "storage = " << aForceStore.getName() << endl;
//cout << "computeAverageResiduals size=" << size << "  ave=" << ave << endl;
//cout << "indexs=" << iFX << "  " << iFY << "  " << iFZ << "  " <<  iMX << "  " << iMY << "  " << iMZ <<  endl;

	// GET AVE FORCES
	if(iFX>=0) rFAve[0] = ave[iFX];
	if(iFY>=0) rFAve[1] = ave[iFY];
	if(iFZ>=0) rFAve[2] = ave[iFZ];
	
	// GET AVE MOMENTS
	if(iMX>=0) rMAve[0] = ave[iMX];
	if(iMY>=0) rMAve[1] = ave[iMY];
	if(iMZ>=0) rMAve[2] = ave[iMZ];
//cout << "forces= " << rFAve <<  endl;
//cout << "moments=" << rMAve <<  endl;

}
void RRATool::
adjustCOMToReduceResiduals(SimTK::State& s, const Storage &qStore, const Storage &uStore)
{
	// Create a states storage from q's and u's
	Storage *statesStore = AnalyzeTool::createStatesStorageFromCoordinatesAndSpeeds(*_model, qStore, uStore);

	double ti = _ti;
	double tf = _tf;
	if(_initialTimeForCOMAdjustment!=-1 || _finalTimeForCOMAdjustment!=-1) {
		ti = _initialTimeForCOMAdjustment;
		tf = _finalTimeForCOMAdjustment;
	}

	Array<double> FAve(0.0,3),MAve(0.0,3);

	double actualTi, actualTf;
	statesStore->getTime(statesStore->findIndex(ti),actualTi);
	statesStore->getTime(statesStore->findIndex(tf),actualTf);
	cout<<"\nNote: requested COM adjustment time range "<<ti<<" - "<<tf<<" clamped to nearest available data times "<<actualTi<<" - "<<actualTf<<endl;

	computeAverageResiduals(s, *_model, ti, tf, *statesStore, FAve, MAve);
	cout<<"Average residuals before adjusting "<<_adjustedCOMBody<<" COM:"<<endl;
	cout<<"FX="<<FAve[0]<<" FY="<<FAve[1]<<" FZ="<<FAve[2]<<endl;
	cout<<"MX="<<MAve[0]<<" MY="<<MAve[1]<<" MZ="<<MAve[2]<<endl<<endl;

    SimTK::Vector  restoreStates(s.getNY());
    restoreStates = s.getY();

	adjustCOMToReduceResiduals(FAve,MAve);
	SimTK::State &si = _model->initSystem();

    si.updY() = restoreStates;
    _model->getMultibodySystem().realize(si, Stage::Position );
    
	computeAverageResiduals(si, *_model, ti, tf, *statesStore, FAve, MAve);
	cout<<"Average residuals after adjusting "<<_adjustedCOMBody<<" COM:"<<endl;
	cout<<"FX="<<FAve[0]<<" FY="<<FAve[1]<<" FZ="<<FAve[2]<<endl;
	cout<<"MX="<<MAve[0]<<" MY="<<MAve[1]<<" MZ="<<MAve[2]<<endl<<endl;

	delete statesStore;
}

// Uses an inverse dynamics analysis to compute average residuals
void RRATool::
computeAverageResiduals(SimTK::State& s, Model &aModel,double aTi,double aTf,const Storage &aStatesStore,OpenSim::Array<double>& rFAve,OpenSim::Array<double>& rMAve)
{
	// Turn off whatever's currently there (but remember whether it was on/off)
	AnalysisSet& analysisSet = aModel.updAnalysisSet();
	Array<bool> analysisSetOn = analysisSet.getOn();
	analysisSet.setOn(false);

	// add inverse dynamics analysis
	InverseDynamics* inverseDynamics = new InverseDynamics(&aModel);
	aModel.addAnalysis(inverseDynamics);
    inverseDynamics->setModel(aModel);

	int iInitial = aStatesStore.findIndex(aTi);
	int iFinal = aStatesStore.findIndex(aTf);
	aStatesStore.getTime(iInitial,aTi);
	aStatesStore.getTime(iFinal,aTf);
    
    aModel.getMultibodySystem().realize(s, Stage::Position );

	cout << "\nComputing average residuals between " << aTi << " and " << aTf << endl;
	AnalyzeTool::run(s, aModel, iInitial, iFinal, aStatesStore, false);


	computeAverageResiduals(*inverseDynamics->getStorage(),rFAve,rMAve);

//cout << "\nrFAve= " << rFAve << endl;
//cout << "\nrMAve= " << rMAve << endl;


	aModel.removeAnalysis(inverseDynamics);	// This deletes the analysis as well

	// Turn off whatever's currently there
	analysisSet.setOn(analysisSetOn);

}
//_____________________________________________________________________________
/**
 * Adjust the center of mass to reduce any DC offsets in MX and MZ.
 *
 * @param aFAve The average residual forces.  The dimension of aFAve should
 * be 3.
 * @param aMAve The average residual moments.  The dimension of aMAve should
 * be 3.
 */
void RRATool::
adjustCOMToReduceResiduals(const OpenSim::Array<double> &aFAve,const OpenSim::Array<double> &aMAve)
{
	// CHECK SIZE
	assert(aFAve.getSize()==3 && aMAve.getSize()==3);

	// GRAVITY
	Vec3 g = _model->getGravity();

	// COMPUTE SEGMENT WEIGHT
	Body *body = &_model->updBodySet().get(_adjustedCOMBody);
	double bodyMass = body->getMass();
	double bodyWeight = fabs(g[1])*bodyMass;
	if(bodyWeight<SimTK::Zero) {
		cout<<"\nRRATool.adjustCOMToReduceResiduals: ERR- ";
		cout<<_adjustedCOMBody<<" has no weight.\n";
		return;
	}
	
	//---- COM CHANGE ----
	double limit = 0.100;
	double dx =  aMAve[2] / bodyWeight;
	double dz = -aMAve[0] / bodyWeight;
	if(dz > limit) dz = limit;
	if(dz < -limit) dz = -limit;
	if(dx > limit) dx = limit;
	if(dx < -limit) dx = -limit;

	cout<<"RRATool.adjustCOMToReduceResiduals:\n";
	cout<<_adjustedCOMBody<<" weight = "<<bodyWeight<<"\n";
	cout<<"dx="<<dx<<", dz="<<dz<<endl;

	// GET EXISTING COM
	Vec3 com;
	body->getMassCenter(com);

	// COMPUTE ALTERED COM
	com[0] -= dx;
	com[2] -= dz;


	// ALTER THE MODEL
	body->setMassCenter(com);

	//---- MASS CHANGE ----
	// Get recommended mass change.
	double dmass = aFAve[1] / g[1];
	cout<<"\ndmass = "<<dmass<<endl;
	// Loop through bodies
	int i;
	int nb = _model->getNumBodies();
	double massTotal=0.0;
	Array<double> mass(0.0,nb),massChange(0.0,nb),massNew(0.0,nb);
	const BodySet & bodySet = _model->getBodySet();
	nb = bodySet.getSize();
	for(i=0;i<nb;i++) {
		mass[i] = bodySet[i].getMass();
		massTotal += mass[i];
	}
	cout<<"\nRecommended mass adjustments:"<<endl;
	for(i=0;i<nb;i++) {
		massChange[i] = dmass * mass[i]/massTotal;
		massNew[i] = mass[i] + massChange[i];
		cout<<bodySet[i].getName()<<":  orig mass = "<<mass[i]<<", new mass = "<<massNew[i]<<endl;
	}
}

//_____________________________________________________________________________
/**
 * Add Actuation/Kinematics analyses if necessary
 */
void RRATool::
addNecessaryAnalyses()
{
	int stepInterval = 1;
	AnalysisSet& as = _model->updAnalysisSet();
	// Add Actuation if necessary
	Actuation *act = NULL;
	for(int i=0; i<as.getSize(); i++) 
		if(as.get(i).getType() == "Actuation") { act = (Actuation*)&as.get(i); break; }
	if(!act) {
		std::cout << "No Actuation analysis found in analysis set -- adding one" << std::endl;
		act = new Actuation(_model);
        act->setModel(*_model );
		act->setStepInterval(stepInterval);
		_model->addAnalysis(act);
	}
	// Add Kinematics if necessary
	// NOTE: also checks getPrintResultFiles() so that the Kinematics analysis added from the GUI does not count
	Kinematics *kin = NULL;
	for(int i=0; i<as.getSize(); i++) 
		if(as.get(i).getType() == "Kinematics" && as.get(i).getPrintResultFiles()) { kin = (Kinematics*)&as.get(i); break; }
	if(!kin) {
		std::cout << "No Kinematics analysis found in analysis set -- adding one" << std::endl;
		kin = new Kinematics(_model);
        kin->setModel(*_model );
		kin->setStepInterval(stepInterval);
		kin->getPositionStorage()->setWriteSIMMHeader(true);
		_model->addAnalysis(kin);
	} else {
		kin->getPositionStorage()->setWriteSIMMHeader(true);
	}
}

//_____________________________________________________________________________
/**
 * Set controls to use steps except for the residuals, which use linear
 * interpolation.  This method also sets the min and max values for
 * the controls.
 *
 * @param aRRAControlSet Controls that were output by an RRA pass.
 * @param rControlSet Controls for the current run of CMC.
 * @todo The residuals are assumed to be the first 6 actuators.  This
 * needs to be made more general.  And, the control bounds need to
 * be set elsewhere, preferably in a file.
 */
void RRATool::
initializeControlSetUsingConstraints(
	const ControlSet *aRRAControlSet,const ControlSet *aControlConstraints, ControlSet& rControlSet)
{
	// Initialize control set with control constraints file
	
	int size = rControlSet.getSize();
	if(aControlConstraints) {
		for(int i=0;i<size;i++) {
            int index = aControlConstraints->getIndex(rControlSet.get(i).getName());
            if(index == -1) {
                // backwards compatibility with old version of OpenSim that appended the 
                //                 //  control suffix to the name of the control
                index = aControlConstraints->getIndex(rControlSet.get(i).getName()+".excitation");
            }
            if( index == -1 ) {
                 cout << "Cannot find control constraint for: " << rControlSet.get(i).getName() << endl;
            } else {
                rControlSet.set(i, (Control*)aControlConstraints->get(index).copy() );
            }
        }
	}

	// FOR RESIDUAL CONTROLS, SET TO USE LINEAR INTERPOLATION
	if(aRRAControlSet!=NULL) {
		OPENSIM_FUNCTION_NOT_IMPLEMENTED();
		// Need to make sure code below still works after changes to controls/control constraints
#if 0
		size = aRRAControlSet->getSize();
		for(i=0;i<size;i++){
			string rraControlName = aRRAControlSet->get(i).getName();
			ControlLinear *control;
			try {
				control = (ControlLinear*)&rControlSet->get(rraControlName);
			} catch(Exception x) {
				continue;
			}
			if(control==NULL) continue;
			control->setUseSteps(false);
			cout<<"Set "<<rraControlName<<" to use linear interpolation.\n";
		}
#endif
	}	
}
//_____________________________________________________________________________
/**
 * Create a set of control constraints based on an RRA solution.
 * If RRA (Residual Reduction Algorithm) was run to compute or reduce the 
 * residuals as a preprocessing step, those residuals need to be applied 
 * during the CMC run.  They are applied by reading in the residuals computed
 * during RRA and then using these controls to place narrow constraints on 
 * the controls for the residual actuators active during the CMC run.
 *
 * @param aControlConstraints Constraints based on a previous RRA solution
 * to be used during this run of CMC.
 */
ControlSet* RRATool::
constructRRAControlSet(ControlSet *aControlConstraints)
{
	if(_rraControlsFileName=="") return(NULL);
	
	OPENSIM_FUNCTION_NOT_IMPLEMENTED();
	// Need to make sure code below still works after changes to controls/control constraints
#if 0
	int i;
	ControlLinear *controlConstraint=NULL;
	string rraControlName,cmcControlName;
	ControlLinear *rraControl=NULL;

	// LOAD RRA CONTROLS
	ControlSet *rraControlSet=NULL;
	rraControlSet = new ControlSet(_rraControlsFileName);

	// Loop through controls looking for corresponding actuators
	int nrra = rraControlSet->getSize();
	for(i=0;i<nrra;i++) {
		rraControl = (ControlLinear*)&rraControlSet->get(i);
		if(rraControl==NULL) continue;
		rraControlName = rraControl->getName();

		// Does control exist in the model?
		// TODO- We are not using this functionality at the moment,
		// so I'm going to comment this out.
		//int index = _model->getControlIndex(rraControlName);
		int index = 0;

		// Add a constraint based on the rra control 
		if(index>=0) {

			// Create control constraint set if necessary
			if(aControlConstraints==NULL) {
				aControlConstraints = new ControlSet();
				aControlConstraints->setName("ControlConstraints");
			}

			// Get control constraint
			controlConstraint = (ControlLinear*)&&aControlConstraints->get(rraControlName);
			// Control constraint already exists, so clear the existing nodes
			if(controlConstraint!=NULL) {
					controlConstraint->getNodeArray().setSize(0);
			// Make a new control constraint
			} else {
				controlConstraint = new ControlLinear();
				controlConstraint->setName(rraControlName);
				aControlConstraints->append(controlConstraint);
			}

			// Set max and min values
			ArrayPtrs<ControlLinearNode> &nodes = rraControl->getNodeArray();
			int j,nnodes = nodes.getSize();
			double t,x,max,min,dx;
			for(j=0;j<nnodes;j++) {
				t = nodes[j]->getTime();
				x = nodes[j]->getValue();
				dx = 1.00 * (nodes[j]->getMax() - nodes[j]->getMin());
				max = x + dx;
				min = x - dx;
				controlConstraint->setControlValue(t,x);
				controlConstraint->setControlValueMax(t,max);
				controlConstraint->setControlValueMin(t,min);
				//cout<<controlConstraint->getName()<<": t="<<t<<" min="<<min<<" x="<<x<<" max="<<max<<endl;
			}
		}
	}

	// Print out modified control constraints
	//if(aControlConstraints!=NULL) aControlConstraints->print("cmc_aControlConstraints_check.xml");

	return(rraControlSet);
#endif
}

//_____________________________________________________________________________
/**
 * Get a pointer to the Storage object holding forces. This's a utility routine used 
 * by the SimTrack GUI primarily to get access to residuals while running RRA.
 *
 * User-beware, the storage will get out of scope and potentially get deleted when the 
 * analysis is done, so no assumptions about the lifetime of the returned storage object 
 * outside the owning analysis should be made.
 */
Storage& RRATool::getForceStorage(){
		Actuation& actuation = (Actuation&)_model->getAnalysisSet().get("Actuation");
		return *actuation.getForceStorage();
}

void RRATool::setOriginalForceSet(const ForceSet &aForceSet) {
	_originalForceSet = aForceSet;
}

//_____________________________________________________________________________
/**
 * Override default implementation by object to intercept and fix the XML node
 * underneath the tool to match current version, issue warning for unexpected inputs
 */
/*virtual*/ void RRATool::updateFromXMLNode()
{
	int documentVersion = getDocument()->getDocumentVersion();
	std::string controlsFileName ="";
	if ( documentVersion < XMLDocument::getLatestVersion()){
		// Replace names of properties
		if (_node!=NULL && documentVersion<20301){
			// Check that no unexpected settings are used for RRA that was really CMC, we'll do this by checking that
			// replace_force_set is true
			// cmc_time_window is .001
			// solve_for_equilibrium_for_auxiliary_states is off
			// ControllerSet is empty
			SimTK::Xml::Document doc = SimTK::Xml::Document(getDocumentFileName());
			Xml::Element oldRoot = doc.getRootElement();
			Xml::Element toolNode;
			bool isCMCTool = true;
			if (oldRoot.getElementTag()=="OpenSimDocument"){
				Xml::element_iterator iterTool(oldRoot.element_begin("CMCTool"));
				if (iterTool==oldRoot.element_end()){
					isCMCTool = false;
				}
				else
					toolNode = *iterTool;
			}
			else { // older document that had no OpenSimDocument tag
				toolNode = oldRoot;
				isCMCTool = (oldRoot.getElementTag()=="CMCTool");
			}
			if (isCMCTool){
				Xml::element_iterator replace_force_setIter(toolNode.element_begin("replace_force_set"));
				if (replace_force_setIter != toolNode.element_end()){
					String replace_forcesStr = replace_force_setIter->getValueAs<String>();
					if (replace_forcesStr.toLower()!= "true") cout << "Warn: old RRA setup file has replace_force_set set to false, will be ignored" << endl;
				}
				Xml::element_iterator timeWindowIter(toolNode.element_begin("cmc_time_window"));
				if (timeWindowIter != toolNode.element_end()){
					double timeWindow = timeWindowIter->getValueAs<double>();
					if (timeWindow!= .001) cout << "Warn: old setup file has cmc_time_window set to " << timeWindow << ", will be ignored and .001 used instead" << endl;
				}
			}
		}
	}
	AbstractTool::updateFromXMLNode();
}