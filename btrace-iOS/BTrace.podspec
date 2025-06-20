#
# Be sure to run `pod lib lint BTrace.podspec' to ensure this is a
# valid spec before submitting.
#
# Any lines starting with a # are optional, but their use is encouraged
# To learn more about a Podspec see https://guides.cocoapods.org/syntax/podspec.html
#

Pod::Spec.new do |s|
  s.name             = 'BTrace'
  s.version          = '0.1.0'
  s.summary          = 'A short description of BTrace.'

# This description is used to generate tags and improve search results.
#   * Think: What does it do? Why did you write it? What is the focus?
#   * Try to keep it short, snappy and to the point.
#   * Write the description between the DESC delimiters below.
#   * Finally, don't worry about the indent, CocoaPods strips it!

  s.description      = <<-DESC
TODO: Add long description of the pod here.
                       DESC

  s.homepage         = 'https://github.com/bytedance/btrace'
  # s.screenshots     = 'www.example.com/screenshots_1', 'www.example.com/screenshots_2'
  s.license          = { :type => 'MIT', :file => 'LICENSE' }
  s.author           = { 'bytedance' => '' }
  s.source           = { :git => 'https://github.com/bytedance/BTrace.git', :tag => s.version.to_s }
  # s.social_media_url = 'https://twitter.com/<TWITTER_USERNAME>'

  s.ios.deployment_target = '12.0'
  s.default_subspecs = 'BTrace'
  
  s.ios.library = 'sqlite3'
  
  # BTrace
  s.subspec 'BTrace' do |ss|
    ss.source_files = 'BTrace/Classes/**/*.{h,hpp,c,cc,m,mm,cpp}'
    ss.public_header_files = 'BTrace/Classes/BTrace/Core/**/*.h'
    ss.pod_target_xcconfig = { 'CLANG_CXX_LANGUAGE_STANDARD' => 'gnu++17' }
    ss.dependency 'fishhook'
    ss.dependency 'Stinger'
  end
end
