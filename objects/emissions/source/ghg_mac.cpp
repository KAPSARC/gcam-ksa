/*! 
* \file ghg_mac.cpp
* \ingroup objects
* \brief GHGMac source file.
* \author Nick Fernandez
*/

#include "util/base/include/definitions.h"
#include <string>
#include <iostream>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/dom/DOMNodeList.hpp>

#include "emissions/include/ghg_mac.h"
#include "util/base/include/xml_helper.h"
#include "util/curves/include/point_set_curve.h"
#include "util/curves/include/curve.h"
#include "util/curves/include/explicit_point_set.h"
#include "util/curves/include/xy_data_point.h"
#include "marketplace/include/marketplace.h"
#include "containers/include/scenario.h"

using namespace std;
using namespace xercesc;

extern Scenario* scenario;

const string GhgMAC::XML_NAME = "MAC";

//! Default constructor.
GhgMAC::GhgMAC(){
    const Modeltime* modeltime = scenario->getModeltime();

	phaseIn = 1;
	finalReduction = 0;
	finalReductionYear = modeltime->getEndYear();
	fuelShiftRange = 0;
	noBelowZero = false;
    baseCostYear = modeltime->getper_to_yr( modeltime->getBasePeriod() );
    costReductionRate = 0;
}

//! Destructor. auto_ptr automatically deallocated the curve. 
GhgMAC::~GhgMAC(){
}

//! Copy constructor.
GhgMAC::GhgMAC( const GhgMAC& other ){
    copy( other );
}

//! Assignment operator.
GhgMAC& GhgMAC::operator=( const GhgMAC& other ){
    if( this != &other ){
        // If there was a destructor it would need to be called here.
        copy( other );
    }
    return *this;
}

//! Copy helper function.
void GhgMAC::copy( const GhgMAC& other ){
    macCurve.reset( other.macCurve->clone() );
    
    // Copy member variables as well
    phaseIn = other.phaseIn;
    fuelShiftRange = other.fuelShiftRange;
    costReductionRate = other.costReductionRate;
    baseCostYear = other.baseCostYear;
    finalReduction = other.finalReduction;
    finalReductionYear = other.finalReductionYear;
    noBelowZero = other.noBelowZero;
    curveShiftFuelName = other.curveShiftFuelName;
}

//! Clone function which returns a deep copy of the GhgMAC.
GhgMAC* GhgMAC::clone() const {
    return new GhgMAC( *this );
}

/*! \brief Get the XML node name for output to XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* This function may be virtual to be overridden by derived class pointers.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME.
*/
const std::string& GhgMAC::getXMLName() const {
	return XML_NAME;
}

/*! \brief Get the XML node name in static form for comparison when parsing XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* The "==" operator that is used when parsing, required this second function to return static.
* \note A function cannot be static and virtual.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME as a static.
*/
const std::string& GhgMAC::getXMLNameStatic() {
	return XML_NAME;
}
/*! \brief Reads in a series of data points and creates a MAC curve from those points
* \detailed The x value of the data points is the carbon price.  The y- value is the amount of 
* reduction in emissions (0= none, 1= completely reduced) The curve that is created is 
* piecewise linear, so that the reduction for any carbon price between two read-in points
* will be found using linear interpolation.
* \author Nick Fernandez and Josh Lurz
* \param node Current node in the DOM tree. 
*/
void GhgMAC::XMLParse( const xercesc::DOMNode* node ){
	/*! \pre Assume we are passed a valid node. */
	assert( node );

	DOMNodeList* nodeList = node->getChildNodes();
	ExplicitPointSet* currPoints = new ExplicitPointSet();
	for( unsigned int i = 0; i < nodeList->getLength(); i++ ) {
		DOMNode* curr = nodeList->item( i );
		const string nodeName = XMLHelper<string>::safeTranscode( curr->getNodeName() );		
        if( nodeName == "#text" ){
            continue;
        }
		else if ( nodeName == "phaseIn" ){
			 phaseIn = XMLHelper<double>::getValue( curr);
		}
		else if ( nodeName == "costReductionRate" ){
			 costReductionRate = XMLHelper<double>::getValue( curr);
		}
		else if ( nodeName == "baseCostYear"){
			baseCostYear = XMLHelper<int>::getValue( curr);
 		}
		else if ( nodeName == "fuelShiftRange" ){
			 fuelShiftRange = XMLHelper<double>::getValue( curr);
		}
		else if ( nodeName == "curveShiftFuelName"){
			curveShiftFuelName = XMLHelper<string>::getValue( curr);
		}
		else if ( nodeName == "finalReduction"){
			finalReduction = XMLHelper<double>::getValue( curr);
		}
		else if ( nodeName == "finalReductionYear"){
			finalReductionYear = XMLHelper<int>::getValue( curr);
 		}
		else if ( nodeName == "noBelowZero"){
			noBelowZero = XMLHelper<bool>::getValue( curr);
		}
		else if ( nodeName == "reduction" ){
			double taxVal = XMLHelper<double>::getAttr( curr, "tax" );	
			double reductionVal = XMLHelper<double>::getValue( curr );
			XYDataPoint* currPoint = new XYDataPoint( taxVal, reductionVal );
			currPoints->addPoint( currPoint );
		}
        else {
            ILogger& mainLog = ILogger::getLogger( "main_log" );
            mainLog.setLevel( ILogger::WARNING );
            mainLog << "Unrecognized text string: " << nodeName << " found while parsing " << GhgMAC::XML_NAME << "." << endl;
        }
	}
    // Can't override a MAC curve currently.
	macCurve.reset( new PointSetCurve( currPoints ) );
}

//! Write out the datamembers of the GHGMac in an XML format.
void GhgMAC::toInputXML( ostream& out, Tabs* tabs ) const {
    const Modeltime* modeltime = scenario->getModeltime();

	XMLWriteOpeningTag(getXMLName(), out, tabs, name );
	
	XMLWriteElementCheckDefault( noBelowZero, "noBelowZero", out, tabs, false);
	XMLWriteElementCheckDefault( fuelShiftRange, "fuelShiftRange", out, tabs, 0.0 );
	XMLWriteElementCheckDefault( costReductionRate, "costReductionRate", out, tabs, 0.0 );
	XMLWriteElementCheckDefault( baseCostYear, "baseCostYear", out, tabs, modeltime->getper_to_yr( modeltime->getBasePeriod() ) );
	XMLWriteElementCheckDefault( phaseIn, "phaseIn", out, tabs, 1.0 );
	XMLWriteElementCheckDefault( finalReduction, "finalReduction", out, tabs, 0.0 );
	XMLWriteElementCheckDefault( finalReductionYear, "finalReductionYear", out, tabs, modeltime->getEndYear() );
	const vector<pair<double,double> > pairs = macCurve->getSortedPairs();
	typedef vector<pair<double, double> >::const_iterator PairIterator;
	map<string, double> attrs;
	for( PairIterator currPair = pairs.begin(); currPair != pairs.end(); ++currPair ) {
		attrs[ "tax" ] = currPair->first;
        XMLWriteElementWithAttributes( currPair->second, "reduction", out, tabs, attrs );
	}
	XMLWriteClosingTag( getXMLName(), out, tabs );
}

//! Write out datamembers for debugging in an XML format.
void GhgMAC::toDebugXML( const int period, ostream& out, Tabs* tabs ) const {
	XMLWriteOpeningTag( getXMLName(), out, tabs, name );

	XMLWriteElement( noBelowZero, "noBelowZero", out, tabs);
	XMLWriteElement( fuelShiftRange, "fuelShiftRange", out, tabs );
	XMLWriteElementCheckDefault( costReductionRate, "costReductionRate", out, tabs );
	XMLWriteElementCheckDefault( baseCostYear, "baseCostYear", out, tabs );
	XMLWriteElement( phaseIn, "phaseIn", out, tabs);
	XMLWriteElement( finalReduction, "finalReduction", out, tabs);

	const vector<pair<double,double> > pairs = macCurve->getSortedPairs();
	typedef vector<pair<double, double> >::const_iterator PairIterator;
	for( PairIterator currPair = pairs.begin(); currPair != pairs.end(); ++currPair ) {
		double taxVal= currPair->first;
		double reductionVal= currPair->second;
		XMLWriteElement( taxVal, "taxVal", out, tabs);
		XMLWriteElement( reductionVal, "reductionVal", out, tabs);
	}
	XMLWriteClosingTag( getXMLName(), out, tabs );
}

/*! \brief Perform error checking
* \author  Steve Smith
*/
void GhgMAC::initCalc( const string& ghgName ){

    if ( macCurve->getMaxX() == -DBL_MAX ) {
    	ILogger& mainLog = ILogger::getLogger( "main_log" );
        mainLog.setLevel( ILogger::ERROR );
        mainLog << " MAC for gas "<< ghgName <<" appears to have no data. " << endl;
    }
}

/*! \brief Finds the reduction using a created MAC curve
* \detailed The function finds the current carbon price, and using the pre-defined MAC curve,
* it uses that carbon price as the x- value to find the reduction.
* \author  Nick Fernandez
* \param regionName the name of the region 
* \param period the current period
* \warning MAC curves are all keyed off of CO2 price.
* \todo Add capability to adjust for different GWP in GHG vs MAC.
* \todo make the good name more general instead of hard coding CO2?
* \todo Put the CH4 shift into a separate type of MAC class
*/
double GhgMAC::findReduction( const string& regionName, const int period ){
    const Marketplace* marketplace = scenario->getMarketplace();
    double effectiveCarbonPrice = marketplace->getPrice( "CO2", regionName, period, false );
    if ( effectiveCarbonPrice == Marketplace::NO_MARKET_PRICE ) {
        effectiveCarbonPrice = 0;
    }
    
	// Avoid this calculation if there is no shift to perform
    if ( fuelShiftRange != 0 ) {
        effectiveCarbonPrice = shiftNatGas( period, regionName, effectiveCarbonPrice );
    }
    
    effectiveCarbonPrice *= shiftCostReduction( period, costReductionRate );
    
    double reduction = getMACValue( effectiveCarbonPrice );
    
    if ( noBelowZero && effectiveCarbonPrice < 0 ){
        reduction = 0;
    }
    
    const double maxCO2Tax = macCurve->getMaxX();
    double maxReduction = getMACValue( maxCO2Tax );
    reduction *= adjustPhaseIn( period );

    const Modeltime* modeltime = scenario->getModeltime();
    int finalReductionPeriod = modeltime->getper_to_yr( finalReductionYear );

    if ( finalReduction > maxReduction && finalReductionPeriod > 1 ){
        reduction *= adjustTechCh( period, finalReductionPeriod, maxReduction );
    }
	return reduction;
}

/*! \brief returns a multiplier that phases in the Mac Curve.
*  if for example, the MAC curve was to be phased in over 3 periods, in the base 
*  period, it would return a multiplier of 0, the next period, 1/3, the next period 2/3,
*  and by and after the 3rd period after the base year, it would return a 1.
* \author Nick Fernandez
* \param period the current period
*/
double GhgMAC::adjustPhaseIn( const int period ){
	double mult=1;
	if ((( period - 1 ) < phaseIn) && ( phaseIn >= 1 )){
		 mult = ( period - 1 ) / phaseIn;
	}
	return mult;
}

/*! \brief returns a multiplier that shifts the MAC curve upwards due to technological change.
*  technological change is calculated the same way as for GHG's, with the exception that
*  a direct multiplier is applied to the reductions based on the technological change parameter
* \author Nick Fernandez
* \param period
*/
double GhgMAC::adjustTechCh( const int period, const int finalReductionPeriod, const double maxReduction ){
	double multiplier;
	double change = maxReduction/finalReduction;
	if ( period <= finalReductionPeriod ){
		multiplier = change * ( 1 / ( finalReductionPeriod - 2 ) ) * ( period - 2 );
	}
	else {
        multiplier = change;
	}
    return multiplier;
}

/*! \brief Returns a new effective carbon price that is shifted up or down, based on the Natural Gas price
*  The range of the carbon price shift is set by the read-in parameter, fuelShiftRange.
*  It is a unique parameter that approximates the best fit of each table.
*  It represents the initial range of the shift that occurs between a 50% reduction in the natural gas
*  price and a 200% increase in the natural gas price from the base year (as taken from EPA-EMF results).  
*  This range decreases as the carbon price increases.  This is taken into account by the variable "convergence Factor"
*  NORM_FACTOR is a constant that normalizes the value (1- priceChangeRatio) so that it ranges 
*  from -0.6 at 50% reduction in carbon price to 0.4 at a 200% increase in carbon price 
*  (it is normalized to a range of 1).
* \author Nick Fernandez
* \param period the current period
* \param regionName the region
* \param carbonPrice the current carbon price
*/
double GhgMAC::shiftNatGas( const int period, const string& regionName, const double carbonPrice ){
	const Marketplace* marketplace = scenario->getMarketplace();
	double natGasPrice = marketplace->getPrice( curveShiftFuelName, regionName, period );
	double natGasBasePrice = marketplace->getPrice( curveShiftFuelName, regionName, 1 ); // change prices relative to the period 1

	double priceChangeRatio = 1;
	if ( natGasPrice != 0 ) { 
		priceChangeRatio = natGasBasePrice / natGasPrice;
	}
	
	// The formula below was determined by fitting MAC curves with the parameters in the constants below
	// If this changes due to new MAC curves with price shifts some parameters may need to be read in.
	const double NORM_FACTOR = 3 / 5; // This value was adjusted to fit the EPA-EMF data.
	const double minCarbonPrice = macCurve->getMinX();	// Minimum carbon price used in this MAC curve
	const double maxCarbonPrice = macCurve->getMaxX(); // Maximum carbon price used in this MAC curve

	double convergenceFactor = ( 1 / 2 ) + ( 1 / 2 )*( ( maxCarbonPrice - carbonPrice ) / ( maxCarbonPrice - minCarbonPrice ) );
    double newCarbonPrice = carbonPrice + ( NORM_FACTOR*( 1 - priceChangeRatio ) * fuelShiftRange * convergenceFactor );

    // Reset carbon price if beyond MAC curve values (is this necessary? sjs)
	newCarbonPrice = max( newCarbonPrice, minCarbonPrice );
	newCarbonPrice = min( newCarbonPrice, maxCarbonPrice );

	return newCarbonPrice;
}

/*! \brief returns a multiplier for the carbon price shifted due to technological change
*  This makes reductions cheaper over time with no change in the maximum reduction rate
* \author Steve Smith
* \param costReductionRate annual rate of cost reduction for all points on the curve
* \param period model period
*/
double GhgMAC::shiftCostReduction( const int period, const double costReductionRate ){

    if ( costReductionRate != 0 ) {
        const Modeltime* modeltime = scenario->getModeltime();
        int numberYears = modeltime->getper_to_yr( period ) - baseCostYear;
        // Only adjust if after baseCostYear
        if ( numberYears > 0 ) {
            return  1 / pow( 1 + costReductionRate, numberYears );
        }
	}
    return 1;
}

/*! \brief Get mac curve value
*  Wrapper function that takes care of error handling for MAC curve values.
*  If there is an error, a value of zero is returned and a message is logged.
*  Errors can happen if no MAC curve values are read in, although perhaps other error situations can occur.
* \param carbonPrice carbon price
*/
double GhgMAC::getMACValue( const double carbonPrice ){
    const double maxCO2Tax = macCurve->getMaxX();
    
    // so that getY function won't interpolate beyond last value
    double effectiveCarbonPrice = min( carbonPrice, maxCO2Tax );
    
	double reduction = macCurve->getY( effectiveCarbonPrice );
   
     // Check to see if an error has occurred.
    if ( reduction == -DBL_MAX ) {
    	ILogger& mainLog = ILogger::getLogger( "main_log" );
        mainLog.setLevel( ILogger::ERROR );
        mainLog << " An error occured when evaluating MAC curve for a GHG." << endl;
        reduction = 0;
    }
    
	return reduction;
}
