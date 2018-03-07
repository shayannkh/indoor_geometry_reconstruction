//
// Created by NikoohematS on 23-11-2017.
//

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <LaserPoints.h>
#include "post_processing.h"
#include "indoor_reconstruction.h"
#include "visualization_tools.h"
#include <boost/foreach.hpp>
#include <boost/geometry.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/register/point.hpp>
#include <boost/geometry/geometries/register/linestring.hpp>
#include <boost/geometry/geometries/register/ring.hpp>

/// **** not completed function ****
/// this function check for the adjacency of segments and connect them if there is a gap, considering the given threshold
/// the threshold can be increased by the step size to make sure the connection
void intersect_segments(const LaserPoints &segmented_lp, double start_threshold, double increase_step_size,
                        double stop_threshold, char* output_dir) {

    char str_root[500];
    strcpy (str_root,output_dir); // initialize the str_root with root string

    /// make laserpoints from each segment
    vector<LaserPoints> segments;
    segments = PartitionLpByTag (segmented_lp, SegmentNumberTag);

    vector<Plane> segments_plane;
    vector<DataBounds3D> segments_3db;
    vector<ObjectPoints> segments_minrectangle;
    ObjectPoints plane_corners;
    LineTopology plane_edges;
    LineTopologies planes_edges;
    bool verbose=false;
    for(auto &segment : segments){
        /// make a collection of segment planes
        Plane plane;
        plane = segment.FitPlane (segment[0].SegmentNumber ()); /// fit a plane to the segment
        plane.Number () = segment[0].SegmentNumber ();  /// set the segment number as the plane number
        segments_plane.push_back (plane);


        ///  make a collection of segment databounds
/*        DataBounds3D db3d;
        DataBoundsLaser db;
        db = segment.DeriveDataBounds (1);
        db3d.SetMaximumX (db.Maximum ().X ()); db3d.SetMinimumX (db.Minimum ().X ());
        db3d.SetMaximumY (db.Maximum ().Y ()); db3d.SetMinimumY (db.Minimum ().Y ());
        db3d.SetMaximumZ (db.Maximum ().Z ()); db3d.SetMinimumZ (db.Minimum ().Z ());

        segments_3db.push_back (db3d);  /// not used later*/

        /// VisulizePlane3D is just for double check the planes
        /// can be commented later
        VisulizePlane3D (segment, start_threshold, plane_corners, plane_edges, verbose);
        planes_edges.push_back (plane_edges);
    }
    strcpy (str_root,output_dir);
    plane_corners.Write(strcat(str_root, "planes_corners.objpts"));
    strcpy (str_root,output_dir);
    planes_edges.Write(strcat (str_root, "planes_edges.top"), false);

    /// export minimumrectangle3D
    ObjectPoints obbox_corners;
    //LineTopology obbox_edges;
    LineTopologies obboxes_edges;
    Minimum3DRectangle (obbox_corners, obboxes_edges, segmented_lp);
    strcpy (str_root,output_dir);
    obbox_corners.Write(strcat(str_root, "obbox_corners.objpts"));
    strcpy (str_root,output_dir);
    obboxes_edges.Write(strcat (str_root, "obboxes_edges.top"), false);

    int intersection_cnt = 0;
    int line_number      = 0;
    ObjectPoints      intersected_points;
    LineTopology      intersected_line;
    LineTopologies    intersected_lines ;
    pair<int, int>   disjoint_segment_pair;
    double tmp_start_threshold = start_threshold;
    cout << "#segments: " << segments.size () << "  #planes: " << segments_plane.size () << endl;
    /// checking for intersection of two planes in a threshold distance
    for (int i=0; i<segments.size (); i++){
        PointNumberList pnl1;
        //start_threshold = tmp_start_threshold;
        //pnl1 = segments[i].TaggedPointNumberList (SegmentNumberTag, segments[i][0].SegmentNumber ()); /// wrong results
        pnl1 = segmented_lp.TaggedPointNumberList(SegmentNumberTag, segments[i][0].SegmentNumber ());
        cout << "*SEGMENT1*: " << segments[i][0].SegmentNumber () << "  *PNL1*: " << pnl1.size () << endl;
        for (int j=i+1; j<segments.size (); j++){
            Line3D intersection_line;
            if(Intersect2Planes (segments_plane[i], segments_plane[j], intersection_line)){
                /// check if two planes their 3Dbounds overlaps,
                /// if yes we skip them for intersection, because they already have intersection
                /// we dont need this check because it does the check on axis aligned not oriented box
                //if(!Overlap(segments_3db[i], segments_3db[j])){ /// if they not overlap then check for intersection

                /// intersect two planes to get the positions of intersection
                Position3D  intersection_pos1, intersection_pos2;
                PointNumberList pnl2;

                //pnl2 = segments[j].TaggedPointNumberList (SegmentNumberTag, segments[j][0].SegmentNumber ()); /// wrong results
                pnl2 = segmented_lp.TaggedPointNumberList(SegmentNumberTag, segments[j][0].SegmentNumber ());
                cout << "segment2: " << segments[j][0].SegmentNumber () << " pnl2: " << pnl2.size () << endl;
                /// **** important function ****
                if (segmented_lp.IntersectFaces(pnl1, pnl2, segments_plane[i], segments_plane[j], start_threshold,/// 0.0 is intersection threshold
                                                intersection_pos1, intersection_pos2)) {

                    PointNumber pn1, pn2;
                    Covariance3D cov3d;
                    cov3d = Covariance3D (0, 0, 0, 0, 0, 0);

                    /// convert begin and end-point of line intersect to objpoint
                    ObjectPoint  beginp_objpnt, endp_objpnt;
                    intersection_cnt++;
                    pn1 = PointNumber (intersection_cnt);
                    beginp_objpnt = ObjectPoint (intersection_pos1, pn1, cov3d);

                    intersection_cnt++;
                    pn2 = PointNumber (intersection_cnt);
                    endp_objpnt = ObjectPoint (intersection_pos2, pn2, cov3d);
                    /// create the line_topology
                    line_number++;
                    intersected_line = LineTopology (line_number, 1, pn1, pn2);

                    /// push back points and lines to the collection
                    intersected_points.push_back (beginp_objpnt);
                    intersected_points.push_back (endp_objpnt);
                    intersected_lines.push_back (intersected_line);

                    /// generate the mid point of the intersected_line
                    Position3D middle_point_pos;
                    middle_point_pos = (intersection_pos1 + intersection_pos2) /2; /// ??? is it correct this way
                    PointNumber pn;
                    intersection_cnt++;
                    pn = PointNumber(intersection_cnt);
                    ObjectPoint midp_objpnt;
                    midp_objpnt = ObjectPoint (middle_point_pos, pn, cov3d);
                    intersected_points.push_back (midp_objpnt); /// doesnt need to be connected to the line

                    /// check if the mid_point is inside the oriented bbox of any of the two segments
                    /// if it is then it means the midpoint is inside one of the segments


                }/*else if (start_threshold < stop_threshold){   /// anyway we start with the max threshold
                    disjoint_segment_pair = make_pair(segments[i][0].SegmentNumber (),segments[j][0].SegmentNumber ());
                    j--;
                    start_threshold+=increase_step_size;
                }*/
                //}

            }
        }
    }
    /// debug, check for intersection quality
    strcpy (str_root,output_dir);
    intersected_lines.Write(strcat(str_root, "intersected_lines.top"), false);
    strcpy (str_root,output_dir); ///initializes the str_root with output_dir string
    intersected_points.Write(strcat(str_root, "intersected_points.objpts"));
}


/// intersecting two rectangle using the boost geometry
namespace bg = boost::geometry;

typedef bg::model::point<double, 2, bg::cs::cartesian> boost_point;
typedef bg::model::polygon<boost_point> boost_polygon;
//typedef boost::geometry::model::polygon<boost::geometry::model::d2::point_xy<double> > polygon;
/// intersect two polygons using the boost geometry
//template < typename Polygon, typename Point >
std::deque<boost_polygon> intersect_polygons(ObjectPoints const &poly1_corners, ObjectPoints const &poly2_corners){



    //create the polygons/rectangles
    boost_polygon rectangle1, rectangle2;

    /// make a boost polygon geometry from objectpoints
    for (int i=0; i < poly1_corners.size () ; i++){
        rectangle1.outer ().push_back(boost_point(poly1_corners[i].GetX (), poly1_corners[i].GetY ()));
    }

    /// make the second boost polygon geometry
    for (int i=0; i < poly2_corners.size () ; i++){
        rectangle2.outer ().push_back(boost_point(poly2_corners[i].GetX (), poly2_corners[i].GetY ()));
    }

    //! display it
/*    std::cout << "generated polygons:" << std::endl;
    std::cout << bg::wkt<boost_polygon>(rectangle1) << std::endl;
    std::cout << bg::wkt<boost_polygon>(rectangle2) << std::endl;*/

    /// make sure both rectangles are closed and ordered correctly
    bg::correct (rectangle1);
    bg::correct (rectangle2);
    /// intersection function of boost
    std::deque<boost_polygon> intersected_poly;
    boost::geometry::intersection(rectangle1, rectangle2, intersected_poly);

    /// area function
/*    int i = 0;
    std::cout << "poly1 && poly2:" << std::endl;
    BOOST_FOREACH(boost_polygon const& p, intersected_poly)
    {
        std::cout << i++ << ": " << boost::geometry::area(p) << std::endl;
    }*/
    return intersected_poly;
}

/* loop through almost horizontal segments and check if their oriented bbox overlap, if yes then check their
 * minimum bounding rectangle (MBR) intersect/overlap or not. For saftey of intersection on adjacent segments
 * we resize the MBR to a saller size of 0.98 of original size.
 * If intersection check is positive then the lower segments
 * would be excluded from the list of candidate ceilings.
 * NOTE: there is no check if the minimum bounding rectangle is not null.
 */
vector<LaserPoints> filter_ceil_by_intersection(map<int, double> &horizon_segmetns_centroidheight_map,
                                                vector<LaserPoints> &candidate_ceil_segment_vec,
                                                const vector<pair<ObjectPoints, LineTopology>> &min_rectangle_ceil,
                                                vector<int> &not_ceiling_segments_nr_output,
                                                LaserPoints &not_ceiling_segments_lp_output,
                                                bool is_floor) {

    for (int i=0; i < candidate_ceil_segment_vec.size (); i++) {
        /// get the bbox bounds
        DataBoundsLaser db1_original, db1;
        db1_original = candidate_ceil_segment_vec[i].DeriveDataBounds (0);
        db1 = ShrinkDBounds (db1_original, 0.05);
        /// get the segment number
        int ceil1_segment_nr;
        ceil1_segment_nr = candidate_ceil_segment_vec[i][0].SegmentNumber ();
        /// check if the segment is already excluded or not
        std::vector<int>::iterator it1;
        it1 = find(not_ceiling_segments_nr_output.begin (), not_ceiling_segments_nr_output.end (), ceil1_segment_nr);
        if(it1 !=not_ceiling_segments_nr_output.end ()) continue; //if it1 found in the vector continue with the next segment
        //printf("ceiling candidate1: %d \n", ceil1_segment_nr); // debug
        /// get the centroid height
        double ceil1_centroid_height;
        ceil1_centroid_height = horizon_segmetns_centroidheight_map.find (ceil1_segment_nr)->second;
        /// get the next segment in the list
        for (int j=i+1; j < candidate_ceil_segment_vec.size (); j++){
            /// get the bbox bounds
            DataBoundsLaser db2_original, db2;
            db2_original = candidate_ceil_segment_vec[j].DeriveDataBounds (0);
            db2 = ShrinkDBounds (db2_original, 0.05); // 5cm shrink
            ///get the segment number
            int ceil2_segment_nr;
            ceil2_segment_nr = candidate_ceil_segment_vec[j][0].SegmentNumber ();
            /// check if it is already excluded or not
            std::vector<int>::iterator it2;
            it2 = find(not_ceiling_segments_nr_output.begin (), not_ceiling_segments_nr_output.end (), ceil1_segment_nr);
            if(it2 !=not_ceiling_segments_nr_output.end ()) continue; // if it2 fount in the vector continue with the next segment
            /// get the segment centroid height
            double ceil2_centroid_height;
            ceil2_centroid_height = horizon_segmetns_centroidheight_map.find (ceil2_segment_nr)->second;
            /// check if they have overlap, if yes then exclude the lower segment from the ceiling candidates
            if(OverlapXY (db1, db2)){
                /// check if their minimum bounding rectangles also intersect
                ObjectPoints mbr1_corners, mbr2_corners;
                LineTopology mbr1_edges, mbr2_edges;
                /// derive corners
                mbr1_corners = min_rectangle_ceil[i].first;
                mbr2_corners = min_rectangle_ceil[j].first;
                /// derive edges
                mbr1_edges = min_rectangle_ceil[i].second;
                mbr2_edges = min_rectangle_ceil[j].second;
                /// first we shrink their size
                ObjectPoints mbr1_shrinked_corners, mbr2_shrinked_corners;
                mbr1_shrinked_corners = ScaleRectangle (mbr1_corners, mbr1_edges, 0.98); // 98% of original size
                mbr2_shrinked_corners = ScaleRectangle (mbr2_corners, mbr2_edges, 0.98);
                /// then we check for their intersection in XY plane using boost geometry
                deque<boost_polygon> intersected_polygon;
                intersected_polygon = intersect_polygons (mbr1_shrinked_corners, mbr2_shrinked_corners);
                /// NOTE: there is no check for the correctness of the intersection
                if(!intersected_polygon.empty ()){
                    /// find and remove the lower segment
                    if((ceil2_centroid_height < ceil1_centroid_height) &&
                       (candidate_ceil_segment_vec[j].size () < candidate_ceil_segment_vec[i].size ())){
                        not_ceiling_segments_nr_output.push_back (ceil2_segment_nr);
                        //printf("ceiling candidate2 excluded: %d \n", ceil2_segment_nr); //debug
                    }
                    if((ceil1_centroid_height < ceil2_centroid_height) &&
                       (candidate_ceil_segment_vec[i].size () < candidate_ceil_segment_vec[j].size ())){
                        not_ceiling_segments_nr_output.push_back (ceil1_segment_nr);
                        //printf("ceiling candidate2 excluded: %d \n", ceil2_segment_nr); //debug
                    }

                    /// if we are checking for the floor we remove the upper segment
                    if(is_floor){
                        if((ceil2_centroid_height > ceil1_centroid_height) &&
                           (candidate_ceil_segment_vec[j].size () < candidate_ceil_segment_vec[i].size ())){
                            not_ceiling_segments_nr_output.push_back (ceil2_segment_nr);
                            //printf("floor candidate2 excluded: %d \n", ceil2_segment_nr); //debug
                        }
                        if((ceil1_centroid_height > ceil2_centroid_height) &&
                           (candidate_ceil_segment_vec[i].size () < candidate_ceil_segment_vec[j].size ())){
                            not_ceiling_segments_nr_output.push_back (ceil1_segment_nr);
                            //printf("floor candidate2 excluded: %d \n", ceil2_segment_nr); //debug
                        }
                    }
                }
            }
        }
    }

    vector <LaserPoints> ceil_segment_filterbyintersect_vec;
    LaserPoints ceil_segment_lp_filterbyintersect;
    for (auto &ceil : candidate_ceil_segment_vec){

        std::vector<int>::iterator it;
        it = find(not_ceiling_segments_nr_output.begin (), not_ceiling_segments_nr_output.end (), ceil[0].SegmentNumber ());
        if(it != not_ceiling_segments_nr_output.end ()){ /// if ceil found in not_ceiling_segments_nr
            not_ceiling_segments_lp_output.AddPoints (ceil);
        }else{ //// if not found add the ceil to the final ceilings
            ceil_segment_filterbyintersect_vec.push_back (ceil);
            //if(verbose) ceil_segment_lp_filterbyintersect.AddPoints (ceil);
        }
    }

    return ceil_segment_filterbyintersect_vec;
}


/*struct TwoDPoint {
    double x, y;
};

typedef std::vector<TwoDPoint> TwoDPolygon;

BOOST_GEOMETRY_REGISTER_POINT_2D(TwoDPoint, double, bg::cs::cartesian, x, y)
BOOST_GEOMETRY_REGISTER_RING( std::vector<TwoDPoint> )
//BOOST_GEOMETRY_REGISTER_LINESTRING(TwoDPolygon)

template < typename Polygon, typename Point >
std::deque<Polygon> test_polygon() {

    ObjectPoint objp1, objp2, objp3, objp4;
*//*    ObjectPoints poly1_corners, poly2_corners;
    for (int i=0; i < 4 ; i++){
        Polygon rectangle1 { {poly1_corners[i].GetX (), poly1_corners[i].GetY ()}}; /// this writes just the last i in the polygon
    }*//*

    Polygon rectangle1 { {objp1.GetX (), objp1.GetY ()},
                        {objp2.GetX (), objp2.GetY ()},
                        {objp3.GetX (), objp3.GetY ()},
                        {objp4.GetX (), objp4.GetY ()}};

    Polygon rectangle2 { {objp1.GetX (), objp1.GetY ()},
                         {objp2.GetX (), objp2.GetY ()},
                         {objp3.GetX (), objp3.GetY ()},
                         {objp4.GetX (), objp4.GetY ()}};


    Polygon poly1 { {0.0,  0.0}, {0.0, 1.0}, {1.0, 1.0}, {1.0,  0.0}, {0.05,  0.0}, };
    Polygon poly2 { {0.5, -0.5}, {0.5, 0.5}, {1.5, 0.5}, {1.5, -0.5},  {0.5, -0.5}, };

    std::deque<Polygon> output;
    boost::geometry::intersection(poly1, poly2, output);

    return output;
}

int main_function() {
    for (auto& p : test_polygon< TwoDPolygon, TwoDPoint >())
        std::cout << "Intersection: " << bg::wkt(p) << "\n";
}*/







