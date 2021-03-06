#include<sq_fitting/sq_fitter.h>
#include<tf/transform_listener.h>
#include <Eigen/Eigen>
#include <eigen_conversions/eigen_msg.h>
#include <tf_conversions/tf_eigen.h>

#include<iri_tos_supervoxels/object_segmentation.h>

#include <pcl/filters/radius_outlier_removal.h>

#include <pcl/filters/median_filter.h>
#include <sq_fitting/get_sq_param.h>
#include<visualization_msgs/Marker.h>

#include<pcl/filters/crop_box.h>




SQFitter::SQFitter(ros::NodeHandle &node, const std::string &cloud_topic, const std::string &output_frame, const SQFitter::Parameters &params)
  : table_plane_cloud_(new PointCloud), segmented_objects_cloud_(new PointCloud), objects_on_table_(new PointCloud),
  cloud_(new PointCloud), transformed_cloud_(new PointCloud), filtered_cloud_(new PointCloud), cut_cloud_(new PointCloud), output_frame_(output_frame)
{

  cloud_sub_ = node.subscribe(cloud_topic, 1, &SQFitter::cloud_callback, this);

  table_pub_ = node.advertise<sensor_msgs::PointCloud2>("table",10);
  objects_pub_ = node.advertise<sensor_msgs::PointCloud2>("segmented_objects",10);
  superquadrics_pub_ = node.advertise<sensor_msgs::PointCloud2>("superquadrics",10);
  filtered_cloud_pub_ = node.advertise<sensor_msgs::PointCloud2>("filtered_cloud",10);
  poses_pub_ = node.advertise<geometry_msgs::PoseArray>("sq_poses",10);
  cut_cloud_pub_ = node.advertise<sensor_msgs::PointCloud2>("cut_cloud", 10);
  //sqs_pub_ = node.advertise<sq_fitting::sqArray>("sqs",10);
  center_pub_ = node.advertise<visualization_msgs::Marker>("visualization_marker",10);
  transformed_pub_ = node.advertise<sensor_msgs::PointCloud2>("trnsformed_cloud",10);

  client_=node.serviceClient<iri_tos_supervoxels::object_segmentation>("/iri_tos_supervoxels_alg/object_segmentation");
 //client_param = node.serviceClient<sq_fitting::get_sq_param>("/get_sq_param");
  service_ = node.advertiseService("sqs", &SQFitter::serviceCallback, this);

  this->sq_param_ = params;
  this->seg_param_ = params.seg_params;
  this->initialized = true;
  Objects_.resize(0);
}

bool SQFitter::serviceCallback(sq_fitting::get_sq::Request &req, sq_fitting::get_sq::Response &res)
{
  std::cout<<"Service callback started"<<std::endl;
  std::cout<<"Request received to send back SQ"<<std::endl;
  res.sqs = sqArr_;
  res.table_center = table_center_;
  std::cout<<"Sending back: "<<sqArr_.sqs.size()<<" superquadrics";
  std::cout<<"Table center: "<<table_center_.x <<" "<<table_center_.y<<" "
          <<table_center_.z<<std::endl;
}

void SQFitter::cloud_callback(const sensor_msgs::PointCloud2ConstPtr &input)
{
  std::cout<<"I am calling cloud"<<std::endl;
  pcl::fromROSMsg(*input, *cloud_);
  this->input_msg_ = *input;
  transformFrameCloud(this->cloud_, this->transformed_cloud_);
  pcl::toROSMsg(*transformed_cloud_, transformed_cloud_ros_);

  filterCloud(this->transformed_cloud_, this->filtered_cloud_);
  pcl::toROSMsg(*filtered_cloud_, filtered_cloud_ros_);

  getSegmentedObjectsIRI(this->filtered_cloud_);
  pcl::toROSMsg(*segmented_objects_cloud_, objects_cloud_);


  pcl::toROSMsg(*cut_cloud_, cut_cloud_ros_);

  getSuperquadricParameters(this->params_);
  sampleSuperquadrics(this->params_);
}

void SQFitter::transformFrameCloud(const CloudPtr& cloud_in, CloudPtr& cloud_out)
{

  if(output_frame_ == this->input_msg_.header.frame_id)
  {
    std::cout<<"Ok till now"<<std::endl;
  }
  else
  {

  tf::TransformListener listener;
  tf::StampedTransform transform;
  try{

    listener.waitForTransform( output_frame_,this->input_msg_.header.frame_id,ros::Time(0), ros::Duration(3.0));
    listener.lookupTransform(output_frame_, this->input_msg_.header.frame_id,ros::Time(0), transform);

    geometry_msgs::Pose inter_pose;
    inter_pose.position.x = transform.getOrigin().x();
    inter_pose.position.y = transform.getOrigin().y();
    inter_pose.position.z = transform.getOrigin().z();
    inter_pose.orientation.x = transform.getRotation().getX();
    inter_pose.orientation.y = transform.getRotation().getY();
    inter_pose.orientation.z = transform.getRotation().getZ();
    inter_pose.orientation.w = transform.getRotation().getW();
    Eigen::Affine3d transform_in_eigen;
    tf::poseMsgToEigen(inter_pose, transform_in_eigen);
    pcl::transformPointCloud (*cloud_in, *cloud_out, transform_in_eigen);
    cloud_out->header.frame_id = output_frame_;

  }
  catch (tf::TransformException ex){
        ROS_ERROR("%s",ex.what());
        ros::Duration(1.0).sleep();
      }

  }
}

void SQFitter::transformFrameCloudBack(const CloudPtr& cloud_in, CloudPtr& cloud_out)
{
  if(output_frame_ == this->input_msg_.header.frame_id)
  {
    std::cout<<"Ok till now"<<std::endl;
  }
  else{

  tf::TransformListener listener;
  tf::StampedTransform transform;
  try{

    listener.waitForTransform( this->input_msg_.header.frame_id, output_frame_,ros::Time(0), ros::Duration(3.0));
    listener.lookupTransform(this->input_msg_.header.frame_id, output_frame_,ros::Time(0), transform);

    geometry_msgs::Pose inter_pose;
    inter_pose.position.x = transform.getOrigin().x();
    inter_pose.position.y = transform.getOrigin().y();
    inter_pose.position.z = transform.getOrigin().z();
    inter_pose.orientation.x = transform.getRotation().getX();
    inter_pose.orientation.y = transform.getRotation().getY();
    inter_pose.orientation.z = transform.getRotation().getZ();
    inter_pose.orientation.w = transform.getRotation().getW();
    Eigen::Affine3d transform_in_eigen;
    tf::poseMsgToEigen(inter_pose, transform_in_eigen);
    pcl::transformPointCloud (*cloud_in, *cloud_out, transform_in_eigen);
    cloud_out->header.frame_id = this->input_msg_.header.frame_id;

  }
  catch (tf::TransformException ex){
        ROS_ERROR("%s",ex.what());
        ros::Duration(1.0).sleep();
      }

  }
}

void SQFitter::filterCloud(const CloudPtr& cloud, CloudPtr& filtered_cloud)
{
  CloudPtr cloud_nan(new PointCloud);
  pcl::CropBox<PointT> crop;
  crop.setInputCloud(cloud);
  Eigen::Vector4f min;
  min<<sq_param_.ws_limits[0] ,sq_param_.ws_limits[2],sq_param_.ws_limits[4],1;
  Eigen::Vector4f max;
  max<<sq_param_.ws_limits[1],sq_param_.ws_limits[3], sq_param_.ws_limits[5],1;
  crop.setMin(min);
  crop.setMax(max);
  crop.filter(*cloud_nan);
  std::vector<int> indices;
  pcl::removeNaNFromPointCloud(*cloud_nan, *filtered_cloud, indices);
}

void SQFitter::createCenterMarker(const double x, const double y, const double z)
{

  centerPoint_.header.frame_id = "/world";
  centerPoint_.header.stamp = ros::Time::now();
  centerPoint_.ns = "basic_shape";
  centerPoint_.id = 0;
  centerPoint_.type = visualization_msgs::Marker::SPHERE;
  centerPoint_.action = visualization_msgs::Marker::ADD;
  centerPoint_.pose.position.x = x;
  centerPoint_.pose.position.y = y;
  centerPoint_.pose.position.z = z;
  centerPoint_.pose.orientation.w = 1.0;
  centerPoint_.scale.x = 0.01;
  centerPoint_.scale.y = 0.01;
  centerPoint_.scale.z = 0.01;
  centerPoint_.color.r = 0.0;
  centerPoint_.color.g = 1.0;
  centerPoint_.color.b = 0.0;
  centerPoint_.color.a = 1.0;
  centerPoint_.lifetime = ros::Duration();
}

void SQFitter::filter_StatOutlier(CloudPtr &cloud_in, CloudPtr &cloud_out)
{
  pcl::StatisticalOutlierRemoval<PointT> sor;
  sor.setInputCloud (cloud_in);
  sor.setMeanK (1);
  sor.setStddevMulThresh (0.5);
  sor.filter (*cloud_out);
}

void SQFitter::filter_RadiusOutlier(CloudPtr &cloud_in, CloudPtr &cloud_out)
{
  pcl::RadiusOutlierRemoval<PointT> outrem;
  outrem.setInputCloud(cloud_in);
  outrem.setRadiusSearch(0.05);
  outrem.setMinNeighborsInRadius (1);
  outrem.filter (*cloud_out);
}

void SQFitter::mirror_cloud(CloudPtr &cloud_in, CloudPtr &cloud_out)
{
  double x, y,z;
  getCenter(cloud_in, x, y, z);
  geometry_msgs::Pose pose_in;
  pose_in.position.x = x;
  pose_in.position.y = y;
  pose_in.position.z = z;
  pose_in.orientation.w = 1.0;

  Eigen::Affine3d pose_in_eigen;
  tf::poseMsgToEigen(pose_in, pose_in_eigen);
  Eigen::Affine3d pose_inv = pose_in_eigen.inverse();
  PointCloud::Ptr to_center(new PointCloud);
  PointCloud::Ptr cloud_mirror(new PointCloud);
  PointCloud::Ptr cloud_mirror_trns(new PointCloud);
  pcl::transformPointCloud (*cloud_in, *to_center, pose_inv);

  for(int i=0;i<to_center->points.size();++i)
  {
    PointT point;
    point.x = -(to_center->points.at(i).x);
    point.y = -(to_center->points.at(i).y);
    point.z = -(to_center->points.at(i).z);
    cloud_mirror->points.push_back(point);
  }
  cloud_mirror->header.frame_id = cloud_in->header.frame_id;

  cloud_mirror->width =  cloud_mirror->points.size();
  cloud_mirror->height = 1;
  cloud_mirror->is_dense = true;
  pcl::transformPointCloud (*cloud_mirror, *cloud_mirror_trns, pose_in_eigen);

  for(size_t i=0;i<cloud_mirror_trns->points.size();++i)
  {
    cloud_out->points.push_back(cloud_mirror_trns->points.at(i));
    cloud_out->points.push_back(cloud_in->points.at(i));
  }

  cloud_out->width =  cloud_out->points.size();
  cloud_out->height = 1;
  cloud_out->is_dense = true;
  cloud_out->header.frame_id = cloud_in->header.frame_id;
  geometry_msgs::Pose pose_out;
  transformFrame(pose_in, pose_out);
  createCenterMarker(pose_out.position.x, pose_out.position.y, pose_out.position.z );

}

void SQFitter::getSegmentedObjectsIRI(CloudPtr &cloud)
{
  CloudPtr cloud_t(new PointCloud);
  CloudPtr segmented_objects_cloud_ir(new PointCloud);
  CloudPtr cut_cloud_ir(new PointCloud);
  transformFrameCloudBack(cloud, cloud_t);

  sensor_msgs::PointCloud2 ros_cloud;
  pcl::toROSMsg(*cloud_t, ros_cloud);
  iri_tos_supervoxels::object_segmentation srv;
  srv.request.point_cloud = ros_cloud;

  if(client_.call(srv))
  {
    table_cloud_ = srv.response.plane_cloud;
    std::cout<<"Points in table cloud"<<table_cloud_.data.size()<<std::endl;
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr table_cloud_pcl(new pcl::PointCloud<pcl::PointXYZRGB>);
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr table_cloud_pcl_trns(new pcl::PointCloud<pcl::PointXYZRGB>);
    pcl::fromROSMsg(table_cloud_, *table_cloud_pcl);
    transformFrameCloud(table_cloud_pcl, table_cloud_pcl_trns);
    double x, y,z;
    getCenter(table_cloud_pcl_trns, x, y, z);
    table_center_.x = x;
    table_center_.y = y;
    table_center_.z = z;
    std::cout<<"We found : "<<srv.response.objects.objects.size()<<" objects"<<std::endl;
    segmented_objects_cloud_->points.resize(0);
    cut_cloud_->points.resize(0);
    Objects_.resize(0);
    for(int i=0;i<srv.response.objects.objects.size();++i){
      PointCloud::Ptr tmp(new PointCloud); //Getting individual object cloud
      PointCloud::Ptr before_mirror_ir(new PointCloud);
      PointCloud::Ptr before_mirror(new PointCloud);
      pcl::fromROSMsg(srv.response.objects.objects[i],*before_mirror_ir);
      before_mirror_ir->width = before_mirror_ir->points.size();
      before_mirror_ir->height = 1;
      before_mirror_ir->is_dense = true;


      transformFrameCloud(before_mirror_ir, before_mirror);


      PointCloud::Ptr after_radius_outlier(new PointCloud);
      //filter_StatOutlier(before_mirror, tmp);
      filter_RadiusOutlier(before_mirror, after_radius_outlier);
      filter_StatOutlier(after_radius_outlier, tmp);

      PointCloud::Ptr mirror_tmp(new PointCloud);
      PointCloud::Ptr mirror_filter(new PointCloud);
      mirror_cloud(tmp, mirror_tmp);

      Objects_.push_back(mirror_tmp);
      float r,g,b;
      r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
      g = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
      b = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
      for(int j=0;j<before_mirror->points.size();++j){
        pcl::PointXYZRGB temp_point;
        temp_point.x = before_mirror->points.at(j).x;
        temp_point.y = before_mirror->points.at(j).y;
        temp_point.z = before_mirror->points.at(j).z;
        temp_point.r = r*255;
        temp_point.g = g*255;
        temp_point.b = b*255;
        segmented_objects_cloud_->points.push_back(temp_point);
      }

      for(int j=0;j<mirror_tmp->points.size();++j){
        pcl::PointXYZRGB temp_point;
        temp_point.x = mirror_tmp->points.at(j).x;
        temp_point.y = mirror_tmp->points.at(j).y;
        temp_point.z = mirror_tmp->points.at(j).z;
        temp_point.r = r*255;
        temp_point.g = g*255;
        temp_point.b = b*255;
        cut_cloud_->points.push_back(temp_point);
      }

    }

    segmented_objects_cloud_->header.frame_id = output_frame_;
    segmented_objects_cloud_->width = segmented_objects_cloud_->points.size();
    segmented_objects_cloud_->height = 1;
    segmented_objects_cloud_->is_dense = true;

    cut_cloud_->width = cut_cloud_->points.size();
    cut_cloud_->height = 1;
    cut_cloud_->is_dense = true;
    cut_cloud_->header.frame_id = output_frame_;

  }

  else
    ROS_ERROR("Failed to call service");

}

void SQFitter::transformFrame(const geometry_msgs::Pose &pose_in, geometry_msgs::Pose& pose_out)
{

  if(output_frame_ == this->input_msg_.header.frame_id)
  {
    std::cout<<"Ok till now"<<std::endl;
    pose_out = pose_in;
  }
  else{

  tf::TransformListener listener;
  tf::StampedTransform transform;
  try{

    listener.waitForTransform( output_frame_,this->input_msg_.header.frame_id,ros::Time(0), ros::Duration(3.0));
    listener.lookupTransform(output_frame_, this->input_msg_.header.frame_id,ros::Time(0), transform);


    geometry_msgs::Pose inter_pose;
    inter_pose.position.x = transform.getOrigin().x();
    inter_pose.position.y = transform.getOrigin().y();
    inter_pose.position.z = transform.getOrigin().z();
    inter_pose.orientation.x = transform.getRotation().getX();
    inter_pose.orientation.y = transform.getRotation().getY();
    inter_pose.orientation.z = transform.getRotation().getZ();
    inter_pose.orientation.w = transform.getRotation().getW();
    Eigen::Affine3d transform_in_eigen;
    tf::poseMsgToEigen(inter_pose, transform_in_eigen);

    Eigen::Affine3f pose_in_eigen;
    sq_create_transform(pose_in, pose_in_eigen);

    tf::poseEigenToMsg( transform_in_eigen * pose_in_eigen.cast<double>(), pose_out );


  }
  catch (tf::TransformException ex){
        ROS_ERROR("%s",ex.what());
        ros::Duration(1.0).sleep();
      }

  }
}

void SQFitter::getSegmentedObjects(CloudPtr& cloud)
{
  Segmentation* seg = new Segmentation(cloud, this->seg_param_);
  seg->segment();

  seg->getTablecloud(table_plane_cloud_);
  pcl::toROSMsg(*table_plane_cloud_, table_cloud_);
  table_cloud_.header.seq = 1;
  table_cloud_.header.frame_id = this->input_msg_.header.frame_id;
  table_cloud_.header.stamp = ros::Time::now();

  seg->getObjectsOnTable(objects_on_table_);
  pcl::toROSMsg(*objects_on_table_, objects_on_table_cloud_);
  objects_on_table_cloud_.header.seq = 1;
  objects_on_table_cloud_.header.frame_id = this->input_msg_.header.frame_id;
  objects_on_table_cloud_.header.stamp = ros::Time::now();

  seg->getObjectsCloud(segmented_objects_cloud_);
  pcl::toROSMsg(*segmented_objects_cloud_, objects_cloud_);
  objects_cloud_.header.seq = 1;
  objects_cloud_.header.frame_id = this->input_msg_.header.frame_id;
  objects_cloud_.header.stamp = ros::Time::now();

  //seg->getCutCloud(cut_cloud_);
  pcl::toROSMsg(*cut_cloud_, cut_cloud_ros_);
  cut_cloud_ros_.header.seq = 1;
  cut_cloud_ros_.header.frame_id = this->input_msg_.header.frame_id;
  cut_cloud_ros_.header.stamp = ros::Time::now();


  seg->getObjects(Objects_);
}

void SQFitter::getSuperquadricParameters(std::vector<sq_fitting::sq>& params)
{
  params.resize(0);
  for(int i=0;i<Objects_.size();++i)
  {

    SuperquadricFitting* sq_fit = new SuperquadricFitting(Objects_[i]);
    sq_fitting::sq min_param;
    double min_error;
    sq_fit->fit();
    sq_fit->getMinParams(min_param);
    sq_fit->getMinError(min_error);
    //ROS_INFO("Minimum error: %f ", min_error);
    ROS_INFO("======Parameters for Object[%d]======", i+1);
    ROS_INFO("a1: %f    a2:%f    a3:%f",min_param.a1, min_param.a2, min_param.a3 );
    ROS_INFO("e1: %f    e2:%f    ",min_param.e1, min_param.e2 );
    ROS_INFO("tx: %f    ty:%f    tz:%f",min_param.pose.position.x, min_param.pose.position.y ,min_param.pose.position.z );
    ROS_INFO("rx: %f    ry:%f     rz:%f   rw:%f", min_param.pose.orientation.x, min_param.pose.orientation.y,min_param.pose.orientation.z, min_param.pose.orientation.w);


    params.push_back(min_param);

  }
}

void SQFitter::sampleSuperquadrics(const std::vector<sq_fitting::sq>& params)
{
  CloudPtr sq_cloud_pcl_(new PointCloud);
  poseArr_.poses.resize(0);
  sqArr_.sqs.resize(0);
  sq_cloud_pcl_->points.resize(0);
  for(int i=0;i<params.size();++i)
  {

    poseArr_.poses.push_back(params[i].pose);
    sq_fitting::sq super;
    super.a1 = params[i].a1;
    super.a2 = params[i].a2;
    super.a3 = params[i].a3;
    super.e1 = params[i].e1;
    super.e2 = params[i].e2;
    super.pose = params[i].pose;
    sqArr_.sqs.push_back(super);
    sqArr_.header.frame_id = output_frame_;
    sqArr_.header.stamp = ros::Time::now();

    Sampling* sam = new Sampling(super);

    sam->sample_pilu_fisher();
    CloudPtr sq_cloud(new PointCloud);
    sam->getCloud(sq_cloud);
    for(size_t j=0;j<sq_cloud->points.size();++j)
    {
      PointT tmp;
      tmp.x = sq_cloud->points.at(j).x;
      tmp.y = sq_cloud->points.at(j).y;
      tmp.z = sq_cloud->points.at(j).z;
      tmp.r = sq_cloud->points.at(j).r;
      tmp.g = sq_cloud->points.at(j).g;
      tmp.b = sq_cloud->points.at(j).b;
      sq_cloud_pcl_->points.push_back(tmp);
    }
    delete sam;
  }
  sq_cloud_pcl_->height = 1;
  sq_cloud_pcl_->width = sq_cloud_pcl_->points.size();
  sq_cloud_pcl_->is_dense = true;

  pcl::toROSMsg(*sq_cloud_pcl_, sq_cloud_);
  sq_cloud_.header.seq = 1;
  sq_cloud_.header.frame_id = output_frame_;
  sq_cloud_.header.stamp = ros::Time::now();



  poseArr_.header.frame_id =  output_frame_;
  poseArr_.header.stamp = ros::Time::now();




}

void SQFitter::publishClouds()
{
  transformed_pub_.publish(transformed_cloud_ros_);
  table_pub_.publish(table_cloud_);
  filtered_cloud_pub_.publish(filtered_cloud_ros_);
  objects_pub_.publish(objects_cloud_);

  superquadrics_pub_.publish(sq_cloud_);
  poses_pub_.publish(poseArr_);
  cut_cloud_pub_.publish(cut_cloud_ros_);
  //sqs_pub_.publish(sqArr_);
  center_pub_.publish(centerPoint_);
 }

void SQFitter::fit()
{
  if(this->initialized = true)
  {
    ros::Rate rate(1);
    ROS_INFO("Fitting superquadrics");
    while(ros::ok())
    {
      publishClouds();
      ros::spinOnce();
      rate.sleep();
    }
  }
}


