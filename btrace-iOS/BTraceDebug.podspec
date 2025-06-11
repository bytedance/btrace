Pod::Spec.new do |s|
  s.name             = 'BTraceDebug'
  s.version          = '1.0.0'
  s.summary          = 'TODO: add summary'

  s.description      = <<-DESC
  TODO: add summary
  DESC

  s.homepage         = 'TODO: add homepage'
  s.license          = { :type => 'MIT', :file => 'LICENSE' }
  s.author           = { 'bytedance' => '' }
  s.source           = { :git => 'https://github.com/bytedance/BTrace.git', :tag => s.version.to_s }

  s.ios.deployment_target = '12.0'
  s.default_subspecs = 'Debug'

  s.subspec 'Debug' do |d|
    d.source_files = 'BTraceDebug/Debug/**/*.{h,m,mm,c,cc,cpp}'
    d.private_header_files = 'BTraceDebug/Debug/**/*.{h}'
    d.dependency 'BTrace'
    d.dependency 'GCDWebServer/Core'
  end
end
