( function( window ) {

  'use strict';

  // Closes the sidebar menu
  $("#menu-close").click(function(e) {
      e.preventDefault();
      $("#sidebar-wrapper").toggleClass("active");
  });
  // Opens the sidebar menu
  $("#menu-toggle").click(function(e) {
      e.preventDefault();
      $("#sidebar-wrapper").toggleClass("active");
  });
  // Scrolls to the selected menu item on the page
  $(function() {
      $('a[href*=#]:not([href=#],[data-toggle],[data-target],[data-slide])').click(function() {
          if (location.pathname.replace(/^\//, '') == this.pathname.replace(/^\//, '') || location.hostname == this.hostname) {
              var target = $(this.hash);
              target = target.length ? target : $('[name=' + this.hash.slice(1) + ']');
              if (target.length) {
                  $('html,body').animate({
                      scrollTop: target.offset().top
                  }, 1000);
                  return false;
              }
          }
      });
  });
  //#to-top button appears after scrolling
  var fixed = false;
  $(document).scroll(function() {
      if ($(this).scrollTop() > 250) {
          if (!fixed) {
              fixed = true;
              // $('#to-top').css({position:'fixed', display:'block'});
              $('#to-top').show("slow", function() {
                  $('#to-top').css({
                      position: 'fixed',
                      display: 'block'
                  });
              });
          }
      } else {
          if (fixed) {
              fixed = false;
              $('#to-top').hide("slow", function() {
                  $('#to-top').css({
                      display: 'none'
                  });
              });
          }
      }
  });
  var onMapMouseleaveHandler = function(event) {
      var that = $(this);
      that.on('click', onMapClickHandler);
      that.off('mouseleave', onMapMouseleaveHandler);
      that.find('iframe').css("pointer-events", "none");
  }
  var onMapClickHandler = function(event) {
          var that = $(this);
          // Disable the click handler until the user leaves the map area
          that.off('click', onMapClickHandler);
          // Enable scrolling zoom
          that.find('iframe').css("pointer-events", "auto");
          // Handle the mouse leave event
          that.on('mouseleave', onMapMouseleaveHandler);
      }
      // Enable map zooming with mouse scroll when the user clicks the map
  $('.map').on('click', onMapClickHandler);

})(jQuery); // end of jQuery name space

// =======================================
// Angular JS App functions ++++++++++++++
// =======================================

var app1 = angular.module("app1", []);

app1.controller('doorctrlr', ['$scope', '$http', '$location', function($scope, $http, $location) {

  $scope.bookReg = {};
  $scope.openBooking = false;
  $scope.newBooking = false;
  $scope.bookingReqmsg = "";
  $scope.pollDoor = false;
  $scope.doorReady = false;
  $scope.doorToken = "";
  $scope.doorUnlocked = false;
  $scope.doorUnlockmsg = "Waiting for unlock status";

  var key = "SXGWLZPDOKFIVUHJYTQBNMCAERxswgzldpkoifuvjhtybqmncrea";
  var WelcomeInstruction = "Please press the DOOR BUTTON then wait for instruction to gain access.";

  //pollFunc();

  $scope.knocknock = function() {
    if (!$scope.openBooking) {
      console.log("processing door open request");
      var Baseurl = 'https://agent.electricimp.com/###############/valbooking';
      // Check local storage to see if any data stored for this booking
      if (typeof(Storage) !== 'undefined') {

        var valbooking = window.localStorage.getItem ("room101");

        if (valbooking == null) {
          $scope.openBooking = true;
          $scope.newBooking = true;
        }
        else {
          $http.get(Baseurl + '/'+ valbooking)
          .then(function successCallback(resp1) {
            // Ok we can now process the returned data
            console.log(resp1.data);
            $scope.openBooking = true;

            if (resp1.data.USRv == 1) {
              // we have a valid token
              $scope.newBooking = false;
            }
            else if (resp1.data.USRv == 2) {
              // the token sent for validation is not valid so delete
              var valbooking = window.localStorage.removeItem ("room101");
              $scope.newBooking = true;
            }
            if (!$scope.newBooking) {
              $scope.bookingReqmsg = "Hi! I see your device is already registered. " + WelcomeInstruction;
              $scope.pollDoor = true;
              pollFunc();
            }
          },
          function errorCallback(resp1) {
            // inform the user of any errors received
            console.log(resp1);
          })
          .catch(function(e) {
            // handle errors in processing or in error.
          });
        }
      }
      else {
        throw "Sorry we cannot use your device as requires use of localStorage.";
      }
    }
  }

  $scope.submitbooking = function() {
    var brefenc = $scope.bookReg.ref;
    console.log("processing booking request");
    for (var i = 0; i< $scope.bookReg.email.length; i++) {
      brefenc += $scope.bookReg.email.charCodeAt(i).toString(16);
    }
    console.log(brefenc + " | len" + brefenc.length);
    $http ({
       method   : 'POST',
       url      : 'https://agent.electricimp.com/##############/confirmbooking',
       headers : {'Content-Type' : 'application/x-www-form-urlencoded'},
       data     : brefenc
    })
    .then(function successCallback(resp1) {
        console.log(resp1.data);
        if (resp1.data.USRm.length) {
          $scope.bookingReqmsg = resp1.data.USRm;
        }
        if (resp1.data.USRt) {
          // save in local storage
          if (typeof(Storage) !== 'undefined') {
      			valbooking = window.localStorage.setItem ("room101", resp1.data.USRt);
            $scope.bookingReqmsg = "Your booking has been processed. " + WelcomeInstruction;
            $scope.newBooking = false;
            $scope.pollDoor = true;
            pollFunc();
      		}
      		else {
      			throw "Sorry you cannot use this device as system requires use of localStorage";
          }
        }
      },
      function errorCallback(resp1) {

    })
    .catch(function(e){
      // handle errors in processing or in error.
    });
  }

  $scope.unlockDoor = function() {
    console.log("instructing door unlock");
    $http ({
       method   : 'POST',
       url      : 'https://agent.electricimp.com/##################/unlockrequest',
       headers : {'Content-Type' : 'application/x-www-form-urlencoded'},
       data     : $scope.doorToken
    })
    .then(function successCallback(resp1) {
        console.log(resp1.data);
        $scope.doorUnlockmsg = "Waiting for unlock status";
        $scope.doorUnlocked = true;
        pollDoorOpen($scope.doorToken);
      },
      function errorCallback(resp1) {

    })
    .catch(function(e){
      // handle errors in processing or in error.
    });
  }

  function pollFunc() {
    var gINTERVAL = 3000;
    var canPoll = true;
    if ($scope.pollDoor) {
      var Baseurl = 'https://agent.electricimp.com/##############/chkbutton';
      if (typeof(Storage) !== 'undefined') {

        var valbooking = window.localStorage.getItem ("room101");

        if (valbooking == null) {
          $scope.pollDoor = false;
          $scope.newBooking = true;
        }
        else {
          var startTime = (new Date()).getTime();
          $http.get(Baseurl + '/' + valbooking)
          .then(function successCallback(resp1) {
            console.log(resp1.data);
            if (resp1.data.USRv == 1) {
              // token still valid
              if (resp1.data.USRd) {
                // Door button has been pressed no trigger keypad or button
                console.log("door is ready to open");
                //$scope.bookingReqmsg = "";
                $scope.doorReady = true;
                $scope.doorToken = resp1.data.USRm;
                console.log("door token is: " + $scope.doorToken);
              }
              else {
                var intTime = ((new Date).getTime() - startTime );
                setTimeout(pollFunc, intTime);
              }
            }
            else if (resp1.data.USRv == 2) {
              // token no longer valid
              $scope.pollDoor = false;
              $scope.newBooking = true;
            }
          },
          function errorCallback(resp1) {
            console.log(resp1);
          })
          .catch(function(e){
            // handle errors in processing or in error.
          });
        }
      }
      else {
        throw "Sorry we cannot use your device as requires use of localStorage.";
      }
    }
    /*
    else {
      console.log("D?");
      if (canPoll) setTimeout(pollFunc, gINTERVAL); // ensures the function exucutes
      // reset variables here if necessary
    }
    */
  }

  function pollDoorOpen(doorToken) {
    var gINTERVAL = 2000;
    var canPoll = true;
    var Baseurl = 'https://agent.electricimp.com/##################/chkthedoor';
    var startTime = (new Date()).getTime();
    $http.get(Baseurl + '/' + doorToken)
        .then(function successCallback(resp1) {
          console.log(resp1.data);
          // Check response
          $scope.doorUnlockmsg = resp1.data.USRm;
          if (resp1.data.USRr < 4) {
            var intTime = ((new Date).getTime() - startTime );
            setTimeout(pollDoorOpen.bind(null,doorToken), intTime);
          }
          else {
            $scope.doorReady = false;
            $scope.newBooking = false;
            $scope.openBooking = false;
            $scope.doorToken = "";
            $scope.doorUnlocked = false;
            $scope.pollDoor = false;
            console.log("door polling stopped");
          }
        },
        function errorCallback(resp1) {
          console.log(resp1);
        })
        .catch(function(e){
          // handle errors in processing or in error.
        });
  }

}]);
