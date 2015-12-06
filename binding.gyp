{
	'make_global_settings': [
	    ['CXX','/usr/bin/clang++'],
	    ['LINK','/usr/bin/clang++'],
	  ],
	'variables': {
		'tclconfig%': '<!(tclsh8.5 gyp/tclconfig.tcl)'
	},
	'targets': [
		{
			'target_name': 'tcl',
			'sources': [
				'src/tclbinding.cpp',
				'src/tclnotifier.cpp',
				'src/expose.cpp'
			],
			'variables': {
				'tclthreads': '<!(. <(tclconfig) && echo ${TCL_THREADS})',
				'cxx': '<!(bash gyp/cxx.sh)'
			},
			'conditions': [
				[ 'OS=="mac"', {
					'xcode_settings': {
						'OTHER_CPLUSPLUSFLAGS' : ['-std=c++11','-stdlib=libc++'],
						'OTHER_LDFLAGS': ['-stdlib=libc++'],
						'MACOSX_DEPLOYMENT_TARGET': '10.7'
					}
				} ],
				[ 'cxx!="false"', {
					'cflags': [
						'-std=c++11'
					],
					'defines': [
						'HAS_CXX11'
					]
				} ],
				[ 'tclthreads==1', {
					'defines': [
						'HAS_TCL_THREADS'
					],
					'sources': [
						'src/tclworker.cpp'
					]
				} ],
				[ 'tclthreads==1 and cxx!="false"', {
					'sources': [
						'src/taskrunner.cpp',
						'src/asynchandler.cpp'
					]
				} ]
			],
			'include_dirs': [
				'<!(. <(tclconfig) && echo ${TCL_INCLUDE_SPEC} | sed s/-I//g)',
				'<!(node -e "require(\'nan\')")'
			],
			'link_settings': {
				'libraries': [
					'<!(. <(tclconfig) && echo ${TCL_LIB_SPEC})'
				]
			}
		}
	]
}
