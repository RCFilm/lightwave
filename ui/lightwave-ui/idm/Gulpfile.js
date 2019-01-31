/*
 * Copyright © 2012-2018 VMware, Inc.  All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the “License”); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS, without
 * warranties or conditions of any kind, EITHER EXPRESS OR IMPLIED.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

var gulp = require('gulp'),
    rename = require('gulp-rename'),
    concat = require('gulp-concat'),
    clean = require('gulp-clean'),
    //uglify = require('gulp-uglify'),
    htmlmin = require('gulp-html-minifier'),
    cleanCSS = require('gulp-clean-css');

var version = '1.0.2.0';

var sequence = [
    'lightwave-app-ui-js',
    'lightwave-ui-js-minify',
    'lightwave-ui-vendor-js-minify',
    'lightwave-ui-html-minify',
    'lightwave-ui-vendor-css-minify',
    'lightwave-ui-css-minify',
    'lightwave-ui-copy-assets',
    'lightwaveui-index-copy',
    'lightwaveui-config-copy',
    'bootstrap-css'
];

delete(['./dist/*']);



gulp.task('lightwave-ui-js-minify', function(done) {
    var app_js = 'lightwave-ui.' + version + '.js';
    var dest_js_folder = './dist/idm/js';

    gulp.src([
            './app/src/*.js',
            './app/src/**/*.js',
            './app/src/**/**/*.js',
            './app/src/**/**/**/*.js'
        ])
        .pipe(concat(app_js))
        //.pipe(uglify())
        .pipe(rename({ extname: '.min.js' }))
        .pipe(gulp.dest(dest_js_folder));
	done();
});

gulp.task('lightwave-app-ui-js', function(done) {
    var app_js = 'lightwave-app-ui.' + version + '.js';
    var dest_js_folder = './dist/idm/js';

    gulp.src(['./app/app.js'])
        .pipe(concat(app_js))
        .pipe(gulp.dest(dest_js_folder));
    done();
});

gulp.task('bootstrap-css', function(done) {
    var app_js = 'bootstrap.min.css';
    var dest_js_folder = './dist/idm/css';

    gulp.src(['./app/bootstrap.min.css'])
        .pipe(concat(app_js))
        .pipe(gulp.dest(dest_js_folder));
    done();
});

gulp.task('lightwaveui-index-copy', function(done) {
    var app_js = 'index.html';
    var dest_js_folder = './dist';

    gulp.src(['./index.html'])
        .pipe(concat(app_js))
        .pipe(gulp.dest(dest_js_folder));
    done();
});

gulp.task('lightwaveui-config-copy', function(done) {
    var app_js = 'lightwaveui.json';
    var dest_js_folder = './dist/config';

    gulp.src(['./config/lightwaveui.json'])
        .pipe(concat(app_js))
        .pipe(gulp.dest(dest_js_folder));
    done();
});


gulp.task('lightwave-ui-vendor-js-minify', function(done) {
    var app_js = 'lightwave-ui-vendor.' + version + '.js';
    var dest_js_folder = './dist/idm/js';

    gulp.src([
            './node_modules/jquery/dist/jquery.min.js',
            './node_modules/angular/angular.js',
            './node_modules/angular-bootstrap/ui-bootstrap.min.js',
            './node_modules/angular-bootstrap/ui-bootstrap-tpls.min.js',
            './node_modules/angular-cookies/angular-cookies.js',
            './node_modules/ng-dialog/js/ngDialog.min.js',
            './node_modules/angular-route/angular-route.js',
            './node_modules/jsrsasign/lib/jsrsasign.js',
            './node_modules/jsrsasign/lib/header.js',
            './node_modules/jsrsasign/lib/footer.js',
            './node_modules/jsrsasign/lib/lib.js'
        ])
        .pipe(concat(app_js))
        //.pipe(uglify())
        .pipe(rename({ extname: '.min.js' }))
        .pipe(gulp.dest(dest_js_folder));
	done();
});


gulp.task('lightwave-ui-html-minify', function(done) {
    gulp.src([
        './app/src/**/*.html',
        './app/*.html',
        './app/src/sso/**/*.html',
        './app/src/shared/**/*.html'
        ])
        .pipe(htmlmin({collapseWhitespace: true}))
        .pipe(gulp.dest('./dist/idm'))
	done();
});

gulp.task('lightwave-ui-css-minify', function(done) {
    var app_css = 'lightwave-ui.' + version + '.min.css';
    gulp.src(['./app/app.css'])
        .pipe(cleanCSS({compatibility: 'ie8'}))
        .pipe(rename(app_css))
        .pipe(gulp.dest('./dist/idm/css'));
    done();
});

gulp.task('lightwave-ui-vendor-css-minify', function(done) {
    var app_vendor_css = 'lightwave-ui-vendor.' + version + '.css';
    gulp.src([
            './node_modules/ng-dialog/css/ngDialog.min.css',
            './node_modules/ng-dialog/css/ngDialog-theme-default.min.css'
        ])
        .pipe(concat(app_vendor_css))
        .pipe(cleanCSS({compatibility: 'ie8'}))
        .pipe(rename({ extname: '.min.css' }))
        .pipe(gulp.dest('./dist/idm/css'));
    done();
});

gulp.task('lightwave-ui-copy-assets', function(done) {
    gulp.src(['./app/assets/*.png','./app/assets/*.gif'])
        .pipe(gulp.dest('./dist/idm/assets'));
    done();
});

gulp.task('default', gulp.series(sequence));
