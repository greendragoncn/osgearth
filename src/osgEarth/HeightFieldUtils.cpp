/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2014 Pelican Mapping
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <osgEarth/HeightFieldUtils>
#include <osgEarth/GeoData>
#include <osgEarth/Geoid>
#include <osgEarth/CullingUtils>
#include <osgEarth/ImageUtils>
#include <osg/Notify>

using namespace osgEarth;

float
HeightFieldUtils::getHeightAtPixel(const osg::HeightField* hf, double c, double r, ElevationInterpolation interpolation)
{
    float result = 0.0;
    if (interpolation == INTERP_NEAREST)
    {
        //Nearest interpolation
        result = hf->getHeight((unsigned int)osg::round(c), (unsigned int)osg::round(r));
    }
    else if (interpolation == INTERP_TRIANGULATE)
    {
        //Interpolation to make sure that the interpolated point follows the triangles generated by the 4 parent points
        int rowMin = osg::maximum((int)floor(r), 0);
        int rowMax = osg::maximum(osg::minimum((int)ceil(r), (int)(hf->getNumRows()-1)), 0);
        int colMin = osg::maximum((int)floor(c), 0);
        int colMax = osg::maximum(osg::minimum((int)ceil(c), (int)(hf->getNumColumns()-1)), 0);

        if (rowMin == rowMax)
        {
            if (rowMin < (int)hf->getNumRows()-1)
            {
                rowMax = rowMin + 1;
            }
            else if ( rowMax > 0 )
            {
                rowMin = rowMax - 1;
            }
         }

         if (colMin == colMax)
         {
            if (colMin < (int)hf->getNumColumns()-1)
            {
                colMax = colMin + 1;
            }
            else if ( colMax > 0 )
            {
               colMin = colMax - 1;
            }
         }

        if (rowMin > rowMax) rowMin = rowMax;
        if (colMin > colMax) colMin = colMax;

        float urHeight = hf->getHeight(colMax, rowMax);
        float llHeight = hf->getHeight(colMin, rowMin);
        float ulHeight = hf->getHeight(colMin, rowMax);
        float lrHeight = hf->getHeight(colMax, rowMin);

        //Make sure not to use NoData in the interpolation
        if (urHeight == NO_DATA_VALUE || llHeight == NO_DATA_VALUE || ulHeight == NO_DATA_VALUE || lrHeight == NO_DATA_VALUE)
        {
            return NO_DATA_VALUE;
        }


        //The quad consisting of the 4 corner points can be made into two triangles.
        //The "left" triangle is ll, ur, ul
        //The "right" triangle is ll, lr, ur

        //Determine which triangle the point falls in.
        osg::Vec3d v0, v1, v2;

#if 0
        bool orientation = fabs(llHeight-urHeight) < fabs(ulHeight-lrHeight);
        if ( orientation )
        {
            double dx = c - (double)colMin;
            double dy = r - (double)rowMin;

            // divide along ll->ur
            if (dx > dy)
            {
                //The point lies in the right triangle
                v0.set(colMin, rowMin, llHeight);
                v1.set(colMax, rowMin, lrHeight);
                v2.set(colMax, rowMax, urHeight);
            }
            else
            {
                //The point lies in the left triangle
                v0.set(colMin, rowMin, llHeight);
                v1.set(colMax, rowMax, urHeight);
                v2.set(colMin, rowMax, ulHeight);
            }
        }
        else
        {
            double dx = c - (double)colMin;
            double dy = (double)rowMax - r;

            // divide along ul->lr
            if (dx > dy)
            {
                //The point lies in the right triangle
                v0.set(colMax, rowMin, lrHeight);
                v1.set(colMax, rowMax, urHeight);
                v2.set(colMin, rowMax, ulHeight);
            }
            else
            {
                //The point lies in the left triangle
                v0.set(colMin, rowMin, llHeight);
                v1.set(colMax, rowMin, lrHeight);
                v2.set(colMin, rowMax, ulHeight);
            }
        }
#else
        double dx = c - (double)colMin;
        double dy = r - (double)rowMin;

        if (dx > dy)
        {
            //The point lies in the right triangle
            v0.set(colMin, rowMin, llHeight);
            v1.set(colMax, rowMin, lrHeight);
            v2.set(colMax, rowMax, urHeight);
        }
        else
        {
            //The point lies in the left triangle
            v0.set(colMin, rowMin, llHeight);
            v1.set(colMax, rowMax, urHeight);
            v2.set(colMin, rowMax, ulHeight);
        }
#endif

        //Compute the normal
        osg::Vec3d n = (v1 - v0) ^ (v2 - v0);

        result = ( n.x() * ( c - v0.x() ) + n.y() * ( r - v0.y() ) ) / -n.z() + v0.z();
    }
    else
    {
        //OE_INFO << "getHeightAtPixel: (" << c << ", " << r << ")" << std::endl;
        int rowMin = osg::maximum((int)floor(r), 0);
        int rowMax = osg::maximum(osg::minimum((int)ceil(r), (int)(hf->getNumRows()-1)), 0);
        int colMin = osg::maximum((int)floor(c), 0);
        int colMax = osg::maximum(osg::minimum((int)ceil(c), (int)(hf->getNumColumns()-1)), 0);

        if (rowMin > rowMax) rowMin = rowMax;
        if (colMin > colMax) colMin = colMax;

        float urHeight = hf->getHeight(colMax, rowMax);
        float llHeight = hf->getHeight(colMin, rowMin);
        float ulHeight = hf->getHeight(colMin, rowMax);
        float lrHeight = hf->getHeight(colMax, rowMin);

        //Make sure not to use NoData in the interpolation
        if (urHeight == NO_DATA_VALUE || llHeight == NO_DATA_VALUE || ulHeight == NO_DATA_VALUE || lrHeight == NO_DATA_VALUE)
        {
            return NO_DATA_VALUE;
        }

        //OE_INFO << "Heights (ll, lr, ul, ur) ( " << llHeight << ", " << urHeight << ", " << ulHeight << ", " << urHeight << std::endl;

        if (interpolation == INTERP_BILINEAR)
        {
            //Check for exact value
            if ((colMax == colMin) && (rowMax == rowMin))
            {
                //OE_NOTICE << "Exact value" << std::endl;
                result = hf->getHeight((int)c, (int)r);
            }
            else if (colMax == colMin)
            {
                //OE_NOTICE << "Vertically" << std::endl;
                //Linear interpolate vertically
                result = ((double)rowMax - r) * llHeight + (r - (double)rowMin) * ulHeight;
            }
            else if (rowMax == rowMin)
            {
                //OE_NOTICE << "Horizontally" << std::endl;
                //Linear interpolate horizontally
                result = ((double)colMax - c) * llHeight + (c - (double)colMin) * lrHeight;
            }
            else
            {
                //OE_NOTICE << "Bilinear" << std::endl;
                //Bilinear interpolate
                float r1 = ((double)colMax - c) * llHeight + (c - (double)colMin) * lrHeight;
                float r2 = ((double)colMax - c) * ulHeight + (c - (double)colMin) * urHeight;

                //OE_INFO << "r1, r2 = " << r1 << " , " << r2 << std::endl;

                result = ((double)rowMax - r) * r1 + (r - (double)rowMin) * r2;
            }
        }
        else if (interpolation == INTERP_AVERAGE)
        {
            double x_rem = c - (int)c;
            double y_rem = r - (int)r;

            double w00 = (1.0 - y_rem) * (1.0 - x_rem) * (double)llHeight;
            double w01 = (1.0 - y_rem) * x_rem * (double)lrHeight;
            double w10 = y_rem * (1.0 - x_rem) * (double)ulHeight;
            double w11 = y_rem * x_rem * (double)urHeight;

            result = (float)(w00 + w01 + w10 + w11);
        }
    }

    return result;
}

bool
HeightFieldUtils::getInterpolatedHeight(const osg::HeightField* hf, 
                                        unsigned c, unsigned r, 
                                        float& out_height,
                                        ElevationInterpolation interpolation)
{
    int count = 0;
    float total = 0.0f;
    if ( c > 0 ) {
        total += hf->getHeight(c-1, r);
        count++;
    }
    if ( c < hf->getNumColumns()-1 ) {
        total += hf->getHeight(c+1, r);
        count++;
    }
    if ( r > 0 ) {
        total += hf->getHeight(c, r-1);
        count++;
    }
    if ( r < hf->getNumRows()-1 ) {
        total += hf->getHeight(c, r+1);
        count++;
    }
    if ( count > 0 )
        total /= (float)count;
    else
        return false;

    out_height = total;
    return true;
}

float
HeightFieldUtils::getHeightAtLocation(const osg::HeightField* hf, double x, double y, double llx, double lly, double dx, double dy, ElevationInterpolation interpolation)
{
    //Determine the pixel to sample
    double px = osg::clampBetween( (x - llx) / dx, 0.0, (double)(hf->getNumColumns()-1) );
    double py = osg::clampBetween( (y - lly) / dy, 0.0, (double)(hf->getNumRows()-1) );
    return getHeightAtPixel(hf, px, py, interpolation);
}

float
HeightFieldUtils::getHeightAtNormalizedLocation(const osg::HeightField* input,
                                                double nx, double ny,
                                                ElevationInterpolation interp)
{
    double px = osg::clampBetween(nx, 0.0, 1.0) * (double)(input->getNumColumns() - 1);
    double py = osg::clampBetween(ny, 0.0, 1.0) * (double)(input->getNumRows() - 1);
    return getHeightAtPixel( input, px, py, interp );
}

bool
HeightFieldUtils::getHeightAtNormalizedLocation(const HeightFieldNeighborhood& hood,
                                                double nx, double ny,
                                                double& output,
                                                ElevationInterpolation interp)
{
    osg::ref_ptr<osg::HeightField> hf;
    double nx2, ny2;
    if ( hood.getNeighborForNormalizedLocation(nx, ny, hf, nx2, ny2) )
    {
        double px = osg::clampBetween(nx2, 0.0, 1.0) * (double)(hf->getNumColumns() - 1);
        double py = osg::clampBetween(ny2, 0.0, 1.0) * (double)(hf->getNumRows() - 1);
        output = getHeightAtPixel( hf.get(), px, py, interp );
        return true;
    }
    return false;
}

bool
HeightFieldUtils::getNormalAtNormalizedLocation(const osg::HeightField* input,
                                                double nx, double ny,
                                                osg::Vec3& output,
                                                ElevationInterpolation interp)
{
    double xcells = (double)(input->getNumColumns()-1);
    double ycells = (double)(input->getNumRows()-1);

    double w = input->getXInterval() * xcells * 111000.0;
    double h = input->getYInterval() * ycells * 111000.0;

    double ndx = 1.0/xcells;
    double ndy = 1.0/ycells;

    double xmin = osg::clampAbove( nx-ndx, 0.0 );
    double xmax = osg::clampBelow( nx+ndx, 1.0 );
    double ymin = osg::clampAbove( ny-ndy, 0.0 );
    double ymax = osg::clampBelow( ny+ndy, 1.0 );

    osg::Vec3 west (xmin*w, ny*h, getHeightAtNormalizedLocation(input, xmin, ny, interp));
    osg::Vec3 east (xmax*w, ny*h, getHeightAtNormalizedLocation(input, xmax, ny, interp));
    osg::Vec3 south(nx*w, ymin*h, getHeightAtNormalizedLocation(input, nx, ymin, interp));
    osg::Vec3 north(nx*w, ymax*h, getHeightAtNormalizedLocation(input, nx, ymax, interp));

    output = (west-east) ^ (north-south);
    output.normalize();
    return true;
}

void
HeightFieldUtils::scaleHeightFieldToDegrees( osg::HeightField* hf )
{
    if (hf)
    {
        //The number of degrees in a meter at the equator
        //TODO: adjust this calculation based on the actual EllipsoidModel.
        float scale = 1.0f/111319.0f;

        for (unsigned int i = 0; i < hf->getHeightList().size(); ++i)
        {
            hf->getHeightList()[i] *= scale;
        }
    }
    else
    {
        OE_WARN << "[osgEarth::HeightFieldUtils] scaleHeightFieldToDegrees heightfield is NULL" << std::endl;
    }
}


osg::HeightField*
HeightFieldUtils::createSubSample(osg::HeightField* input, const GeoExtent& inputEx, 
                                  const GeoExtent& outputEx, osgEarth::ElevationInterpolation interpolation)
{
    double div = outputEx.width()/inputEx.width();
    if ( div >= 1.0f )
        return 0L;

    int numCols = input->getNumColumns();
    int numRows = input->getNumRows();

    //float dx = input->getXInterval() * div;
    //float dy = input->getYInterval() * div;

    double xInterval = inputEx.width()  / (double)(input->getNumColumns()-1);
    double yInterval = inputEx.height()  / (double)(input->getNumRows()-1);
    double dx = div * xInterval;
    double dy = div * yInterval;


    osg::HeightField* dest = new osg::HeightField();
    dest->allocate( numCols, numRows );
    dest->setXInterval( dx );
    dest->setYInterval( dy );
    dest->setBorderWidth( input->getBorderWidth() );

    // copy over the skirt height, adjusting it for relative tile size.
    dest->setSkirtHeight( input->getSkirtHeight() * div );

    double x, y;
    int col, row;

    for( x = outputEx.xMin(), col=0; col < numCols; x += dx, col++ )
    {
        for( y = outputEx.yMin(), row=0; row < numRows; y += dy, row++ )
        {
            float height = HeightFieldUtils::getHeightAtLocation( input, x, y, inputEx.xMin(), inputEx.yMin(), xInterval, yInterval, interpolation);
            dest->setHeight( col, row, height );
        }
    }

    osg::Vec3d orig( outputEx.xMin(), outputEx.yMin(), input->getOrigin().z() );
    dest->setOrigin( orig );

    return dest;
}

osg::HeightField*
HeightFieldUtils::resampleHeightField(osg::HeightField*      input,
                                      const GeoExtent&       extent,
                                      int                    newColumns, 
                                      int                    newRows,
                                      ElevationInterpolation interp)
{
    if ( newColumns <= 1 && newRows <= 1 )
        return 0L;

    if ( newColumns == input->getNumColumns() && newRows == (int)input->getNumRows() )
        return input;
        //return new osg::HeightField( *input, osg::CopyOp::DEEP_COPY_ALL );

    double spanX = extent.width(); //(input->getNumColumns()-1) * input->getXInterval();
    double spanY = extent.height(); //(input->getNumRows()-1) * input->getYInterval();
    const osg::Vec3& origin = input->getOrigin();

    double stepX = spanX/(double)(newColumns-1);
    double stepY = spanY/(double)(newRows-1);

    osg::HeightField* output = new osg::HeightField();
    output->allocate( newColumns, newRows );
    output->setXInterval( stepX );
    output->setYInterval( stepY );
    output->setOrigin( origin );
    
    for( int y = 0; y < newRows; ++y )
    {
        for( int x = 0; x < newColumns; ++x )
        {
            double nx = (double)x / (double)(newColumns-1);
            double ny = (double)y / (double)(newRows-1);
            float h = getHeightAtNormalizedLocation( input, nx, ny, interp );
            output->setHeight( x, y, h );
        }
    }

    return output;
}


osg::HeightField*
HeightFieldUtils::createReferenceHeightField(const GeoExtent& ex,
                                             unsigned         numCols,
                                             unsigned         numRows,
                                             bool             expressAsHAE)
{
    osg::HeightField* hf = new osg::HeightField();
    hf->allocate( numCols, numRows );
    hf->setOrigin( osg::Vec3d( ex.xMin(), ex.yMin(), 0.0 ) );
    hf->setXInterval( (ex.xMax() - ex.xMin())/(double)(numCols-1) );
    hf->setYInterval( (ex.yMax() - ex.yMin())/(double)(numRows-1) );

    const VerticalDatum* vdatum = ex.isValid() ? ex.getSRS()->getVerticalDatum() : 0L;

    if ( vdatum && expressAsHAE )
    {
        // need the lat/long extent for geoid queries:
        GeoExtent geodeticExtent = ex.getSRS()->isGeographic() ? ex : ex.transform( ex.getSRS()->getGeographicSRS() );
        double latMin = geodeticExtent.yMin();
        double lonMin = geodeticExtent.xMin();
        double lonInterval = geodeticExtent.width() / (double)(numCols-1);
        double latInterval = geodeticExtent.height() / (double)(numRows-1);

        for( unsigned r=0; r<numRows; ++r )
        {            
            double lat = latMin + latInterval*(double)r;
            for( unsigned c=0; c<numCols; ++c )
            {
                double lon = lonMin + lonInterval*(double)c;
                double offset = vdatum->msl2hae(lat, lon, 0.0);
                hf->setHeight( c, r, offset );
            }
        }
    }
    else
    {
        for(unsigned int i=0; i<hf->getHeightList().size(); i++ )
        {
            hf->getHeightList()[i] = 0.0;
        }
    }

    hf->setBorderWidth( 0 );
    return hf;    
}

void
HeightFieldUtils::resolveInvalidHeights(osg::HeightField* grid,
                                        const GeoExtent&  ex,
                                        float             invalidValue,
                                        const Geoid*      geoid)
{
    if ( geoid )
    {
        // need the lat/long extent for geoid queries:
        unsigned numRows = grid->getNumRows();
        unsigned numCols = grid->getNumColumns();
        GeoExtent geodeticExtent = ex.getSRS()->isGeographic() ? ex : ex.transform( ex.getSRS()->getGeographicSRS() );
        double latMin = geodeticExtent.yMin();
        double lonMin = geodeticExtent.xMin();
        double lonInterval = geodeticExtent.width() / (double)(numCols-1);
        double latInterval = geodeticExtent.height() / (double)(numRows-1);

        for( unsigned r=0; r<numRows; ++r )
        {
            double lat = latMin + latInterval*(double)r;
            for( unsigned c=0; c<numCols; ++c )
            {
                double lon = lonMin + lonInterval*(double)c;
                if ( grid->getHeight(c, r) == invalidValue )
                {
                    grid->setHeight( c, r, geoid->getHeight(lat, lon) );
                }
            }
        }
    }
    else
    {
        for(unsigned int i=0; i<grid->getHeightList().size(); i++ )
        {
            if ( grid->getHeightList()[i] == invalidValue )
            {
                grid->getHeightList()[i] = 0.0;
            }
        }
    }
}

osg::NodeCallback*
HeightFieldUtils::createClusterCullingCallback(osg::HeightField*          grid, 
                                               const osg::EllipsoidModel* et, 
                                               float                      verticalScale )
{
    //This code is a very slightly modified version of the DestinationTile::createClusterCullingCallback in VirtualPlanetBuilder.
    if ( !grid || !et )
        return 0L;

    double globe_radius = et->getRadiusPolar();
    unsigned int numColumns = grid->getNumColumns();
    unsigned int numRows = grid->getNumRows();

    double midLong = grid->getOrigin().x()+grid->getXInterval()*((double)(numColumns-1))*0.5;
    double midLat = grid->getOrigin().y()+grid->getYInterval()*((double)(numRows-1))*0.5;
    double midZ = grid->getOrigin().z();

    double midX,midY;
    et->convertLatLongHeightToXYZ(osg::DegreesToRadians(midLat),osg::DegreesToRadians(midLong),midZ, midX,midY,midZ);

    osg::Vec3 center_position(midX,midY,midZ);
    osg::Vec3 center_normal(midX,midY,midZ);
    center_normal.normalize();

    osg::Vec3 transformed_center_normal = center_normal;

    unsigned int r,c;

    // populate the vertex/normal/texcoord arrays from the grid.
    double orig_X = grid->getOrigin().x();
    double delta_X = grid->getXInterval();
    double orig_Y = grid->getOrigin().y();
    double delta_Y = grid->getYInterval();
    double orig_Z = grid->getOrigin().z();


    float min_dot_product = 1.0f;
    float max_cluster_culling_height = 0.0f;
    float max_cluster_culling_radius = 0.0f;

    for(r=0;r<numRows;++r)
    {
        for(c=0;c<numColumns;++c)
        {
            double X = orig_X + delta_X*(double)c;
            double Y = orig_Y + delta_Y*(double)r;
            double Z = orig_Z + grid->getHeight(c,r) * verticalScale;
            double height = Z;

            et->convertLatLongHeightToXYZ(
                osg::DegreesToRadians(Y), osg::DegreesToRadians(X), Z,
                X, Y, Z);

            osg::Vec3d v(X,Y,Z);
            osg::Vec3 dv = v - center_position;
            double d = sqrt(dv.x()*dv.x() + dv.y()*dv.y() + dv.z()*dv.z());
            double theta = acos( globe_radius/ (globe_radius + fabs(height)) );
            double phi = 2.0 * asin (d*0.5/globe_radius); // d/globe_radius;
            double beta = theta+phi;
            double cutoff = osg::PI_2 - 0.1;

            //log(osg::INFO,"theta="<<theta<<"\tphi="<<phi<<" beta "<<beta);
            if (phi<cutoff && beta<cutoff)
            {
                float local_dot_product = -sin(theta + phi);
                float local_m = globe_radius*( 1.0/ cos(theta+phi) - 1.0);
                float local_radius = static_cast<float>(globe_radius * tan(beta)); // beta*globe_radius;
                min_dot_product = osg::minimum(min_dot_product, local_dot_product);
                max_cluster_culling_height = osg::maximum(max_cluster_culling_height,local_m);      
                max_cluster_culling_radius = osg::maximum(max_cluster_culling_radius,local_radius);
            }
            else
            {
                //log(osg::INFO,"Turning off cluster culling for wrap around tile.");
                return 0;
            }
        }
    }    

    osg::NodeCallback* ccc = ClusterCullingFactory::create(
        center_position + transformed_center_normal*max_cluster_culling_height ,
        transformed_center_normal, 
        min_dot_product,
        max_cluster_culling_radius);

    return ccc;
}


osg::Image*
HeightFieldUtils::convertToNormalMap(const HeightFieldNeighborhood& hood,
                                     const SpatialReference*        hoodSRS)
{
    const osg::HeightField* hf = hood._center.get();

    osg::Image* image;
    image->allocateImage(hf->getNumColumns(), hf->getNumRows(), 1, GL_RGB, GL_UNSIGNED_BYTE);

    double xcells = (double)(hf->getNumColumns()-1);
    double ycells = (double)(hf->getNumRows()-1);
    double xres = 1.0/xcells;
    double yres = 1.0/ycells;

    double tInterval = hf->getYInterval();
    double mPerDegAtEquatorInv = 360.0/(hoodSRS->getEllipsoid()->getRadiusEquator() * 2.0 * osg::PI);
    if ( hoodSRS->isGeographic() )
    {
        tInterval *= mPerDegAtEquatorInv;
    }

    ImageUtils::PixelWriter write(image);
    
    for(int t=0; t<hf->getNumRows(); ++t)
    {
        double sInterval = hf->getXInterval();
        if ( hoodSRS->isGeographic() )
        {
            double lat = osg::DegreesToRadians(hf->getOrigin().y() + hf->getYInterval()*(double)t);
            sInterval *= mPerDegAtEquatorInv * cos(lat);
        }

        for(int s=0; s<hf->getNumColumns(); ++s)
        {
            float centerHeight = hf->getHeight(s, t);

            osg::Vec3f west ( -sInterval, 0, 0 );
            osg::Vec3f east (  sInterval, 0, 0 );
            osg::Vec3f south( 0, -tInterval, 0 );
            osg::Vec3f north( 0,  tInterval, 0 );

            float z;
            z = hood.getHeightAtColumnRow(s-1, t);
            west.z() = z != NO_DATA_VALUE ? z : centerHeight;
            z = hood.getHeightAtColumnRow(s+1, t);
            east.z() = z != NO_DATA_VALUE ? z : centerHeight;
            z = hood.getHeightAtColumnRow(s, t-1);
            south.z() = z != NO_DATA_VALUE ? z : centerHeight;
            z = hood.getHeightAtColumnRow(s, t+1);
            north.z() = z != NO_DATA_VALUE ? z : centerHeight;

            osg::Vec3f n = (east-west) ^ (north-south);

            write( osg::Vec4f(n.x(), n.y(), n.z(), 1.0), s, t);
        }
    }

    return image;
}

/******************************************************************************************/

ReplaceInvalidDataOperator::ReplaceInvalidDataOperator():
_replaceWith(0.0f)
{
}

void
ReplaceInvalidDataOperator::operator ()(osg::HeightField *heightField)
{
    if (heightField && _validDataOperator.valid())
    {
        for (unsigned int i = 0; i < heightField->getHeightList().size(); ++i)
        {
            float elevation = heightField->getHeightList()[i];
            if (!(*_validDataOperator)(elevation))
            {
                heightField->getHeightList()[i] = _replaceWith;
            }
        }
    }
}


/******************************************************************************************/
FillNoDataOperator::FillNoDataOperator():
_defaultValue(0.0f)
{
}

void
FillNoDataOperator::operator ()(osg::HeightField *heightField)
{
    if (heightField && _validDataOperator.valid())
    {
        for( unsigned int row=0; row < heightField->getNumRows(); row++ )
        {
            for( unsigned int col=0; col < heightField->getNumColumns(); col++ )
            {
                float val = heightField->getHeight(col, row);

                if (!(*_validDataOperator)(val))
                {
                    if ( col > 0 )
                        val = heightField->getHeight(col-1,row);
                    else if ( col <= heightField->getNumColumns()-1 )
                        val = heightField->getHeight(col+1,row);

                    if (!(*_validDataOperator)(val))
                    {
                        if ( row > 0 )
                            val = heightField->getHeight(col, row-1);
                        else if ( row < heightField->getNumRows()-1 )
                            val = heightField->getHeight(col, row+1);
                    }

                    if (!(*_validDataOperator)(val))
                    {
                        val = _defaultValue;
                    }

                    heightField->setHeight( col, row, val );
                }
            }
        }
    }
}
