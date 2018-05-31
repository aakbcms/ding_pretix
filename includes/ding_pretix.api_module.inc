<?php

/**
 * @file
 * Ding Pretix API.
 */

/**
 * Get the shop url for a given event.
 *
 * @param int $nid
 *   The event (node) id.
 *
 * @return string|null
 *   The shop url if any.
 */
function ding_pretix_get_shop_url($nid) {
  $service_settings = variable_get('ding_pretix', []);
  $info = _ding_pretix_get_pretix_node_info($nid);

  if ($info['maintain_copy']) {
    $shop_url = _ding_pretix_get_event_shop_url($service_settings, $info['pretix_slug']);

    return $shop_url;
  }

  return NULL;
}

/**
 * Create event.
 *
 * Uses Pretix's 'clone' endpoint to create a new event with settings copied
 * from the cloned event.
 *
 * @param $node
 * @param $service_settings
 *
 * @return object
 *
 * @throws Exception
 */
function _ding_pretix_create_event($node, $service_settings) {
  $headers = _ding_pretix_built_request_headers($service_settings);
  $body = _ding_pretix_built_request_body($node, 'INSERT');

  $clone_options = [
    'method' => 'POST',
    'headers' => $headers,
    'data' => json_encode($body),
  ];
  $clone_url = _ding_pretix_get_event_endpoint($service_settings) . $service_settings['template_event_slug'] . '/clone/';

  // Create Event via clone.
  $result = _ding_pretix_http_request($clone_url, $clone_options);

  return $result;
}

/**
 * Update the given the event in pretix.
 *
 * @param $node
 * @param $service_settings
 * @param $pretix_slug
 *
 * @return object
 *
 * @throws Exception
 */
function _ding_pretix_update_event($node, $service_settings, $pretix_slug) {
  $headers = _ding_pretix_built_request_headers($service_settings);
  $body = _ding_pretix_built_request_body($node, 'UPDATE');

  $options = [
    'method' => 'PATCH',
    'headers' => $headers,
    'data' => json_encode($body),
  ];
  $url = _ding_pretix_get_event_endpoint($service_settings, $pretix_slug);
  $result = _ding_pretix_http_request($url, $options);

  return $result;
}

/**
 * Delete the given event in pretix.
 *
 * @param $service_settings
 * @param $pretix_slug
 *
 * @return object
 */
function _ding_pretix_delete_event($service_settings, $pretix_slug) {
  $options = [
    'method' => 'DELETE',
    'headers' => _ding_pretix_built_request_headers($service_settings),
  ];
  $url = _ding_pretix_get_event_endpoint($service_settings, $pretix_slug);
  $result = _ding_pretix_http_request($url, $options);

  return $result;
}

/**
 * Update 'live' for the given event in pretix.
 *
 * @param $is_live
 * @param $service_settings
 * @param $pretix_slug
 *
 * @return object
 */
function _ding_pretix_update_live($is_live, $service_settings, $pretix_slug) {
  $options = [
    'method' => 'PATCH',
    'headers' => _ding_pretix_built_request_headers($service_settings),
    'data' => json_encode(['live' => $is_live]),
  ];
  $url = _ding_pretix_get_event_endpoint($service_settings, $pretix_slug);
  $result = _ding_pretix_http_request($url, $options);

  return $result;
}

/**
 * Enable or disable the 'PDF ticket plugin' in pretix.
 *
 * @param $data
 * @param $use_pdf_tickets
 * @param $service_settings
 * @param $pretix_slug
 *
 * @return null|object
 */
function _ding_pretix_update_pdf_plugin($data, $use_pdf_tickets, $service_settings, $pretix_slug) {
  $pdf_plugin_name = 'pretix.plugins.ticketoutputpdf';

  $data = json_decode($data);
  $enabled_plugins = $data->plugins;
  $key = array_search($pdf_plugin_name, $enabled_plugins);

  if ($key !== FALSE && !$use_pdf_tickets) {
    unset($enabled_plugins[$key]);
  }
  elseif ($key === FALSE && $use_pdf_tickets) {
    $enabled_plugins[] = $pdf_plugin_name;
  }

  if ($data->plugins !== $enabled_plugins) {
    $options = [
      'method' => 'PATCH',
      'headers' => _ding_pretix_built_request_headers($service_settings),
      'data' => json_encode(['plugins' => array_values($enabled_plugins)]),
    ];
    $live_url = _ding_pretix_get_event_endpoint($service_settings, $pretix_slug);
    $result = _ding_pretix_http_request($live_url, $options);
  }

  return $result ?? NULL;
}

/**
 * Update the size of the given quota.
 *
 * @param $quota
 * @param $service_settings
 * @param $pretix_slug
 * @param $pretix_capacity
 *
 * @return null|object
 */
function _ding_pretix_update_quota_size($quota, $service_settings, $pretix_slug, $pretix_capacity) {
  if ($quota && $pretix_capacity !== $quota->size) {
    $pretix_capacity = (int) $pretix_capacity;
    $pretix_capacity = $pretix_capacity === 0 ? NULL : $pretix_capacity;

    $options = [
      'method' => 'PATCH',
      'headers' => _ding_pretix_built_request_headers($service_settings),
      'data' => json_encode(['size' => $pretix_capacity]),
    ];
    $quota_endpoint = _ding_pretix_get_event_endpoint($service_settings, $pretix_slug) . 'quotas/' . $quota->id . '/';
    $result = _ding_pretix_http_request($quota_endpoint, $options);
  }

  return $result ?? NULL;
}

/**
 * Get the shared quota for a list of items from pretix.
 *
 * @param $items
 * @param $service_settings
 * @param $pretix_slug
 *
 * @return null
 */
function _ding_pretix_get_quota($items, $service_settings, $pretix_slug) {
  $options = [
    'method' => 'GET',
    'headers' => _ding_pretix_built_request_headers($service_settings),
  ];
  $quota_endpoint = _ding_pretix_get_event_endpoint($service_settings, $pretix_slug) . 'quotas/';
  $result = _ding_pretix_http_request($quota_endpoint, $options);

  $item_ids = [];
  foreach ($items as $item) {
    $item_ids[] = $item->id;
  }

  if (200 === (int) $result->code) {
    $data = json_decode($result->data);
    foreach ($data->results as $result) {
      // We want the quota that is shared by all the items.
      if (array_intersect($item_ids, $result->items) == $item_ids) {
        $quota = $result;
        break;
      }
    }
  }

  return $quota ?? NULL;
}

/**
 * Get all admission items for a given event from pretix.
 *
 * @param $service_settings
 * @param $pretix_slug
 *
 * @return array
 */
function _ding_pretix_get_admission_items($service_settings, $pretix_slug) {
  $options = [
    'method' => 'GET',
    'headers' => _ding_pretix_built_request_headers($service_settings),
  ];
  $item_endpoint = _ding_pretix_get_event_endpoint($service_settings, $pretix_slug) . 'items/';
  $result = _ding_pretix_http_request($item_endpoint, $options);

  $items = [];
  if (200 === (int) $result->code) {
    $data = json_decode($result->data);
    foreach ($data->results as $result) {
      if ($result->admission) {
        $items[] = $result;
      }
    }
  }

  return $items;
}

/**
 * Update the given item in pretix.
 *
 * @param $node
 * @param $item
 * @param $service_settings
 * @param $pretix_slug
 *
 * @return null|object
 */
function _ding_pretix_update_item($node, $item, $service_settings, $pretix_slug) {
  $wrapper = entity_metadata_wrapper('node', $node);
  $price = $wrapper->field_ding_event_price->value();

  if ($item) {
    $options = [
      'method' => 'PATCH',
      'headers' => _ding_pretix_built_request_headers($service_settings),
      'data' => json_encode(['default_price' => (int) $price]),
    ];
    $endpoint = _ding_pretix_get_event_endpoint($service_settings, $pretix_slug) . 'items/' . $item->id . '/';
    $result = _ding_pretix_http_request($endpoint, $options);
  }

  return $result ?? NULL;
}

/**
 * Get the api endpoint for the given event.
 *
 * If the no event is given the create event endpoint for the default
 * organizer is returned.
 *
 * @param $service_settings
 * @param string|null $event_slug
 *
 * @return string
 */
function _ding_pretix_get_event_endpoint($service_settings, $event_slug = NULL) {
  $endpoint = $service_settings['service_url'] . 'api/v1/organizers/' . $service_settings['organizer_slug'] . '/events/';

  if ($event_slug) {
    $endpoint .= $event_slug . '/';
  }

  return $endpoint;
}

/**
 * Get the pretix shop url for the given event.
 *
 * @param $service_settings
 * @param $event_slug
 *
 * @return string
 */
function _ding_pretix_get_event_shop_url($service_settings, $event_slug) {
  $endpoint = $service_settings['service_url'] . $service_settings['organizer_slug'] . '/' . $event_slug . '/';

  return $endpoint;
}

/**
 * Get the pretix admin url for the given event.
 *
 * @param $service_settings
 * @param $event_slug
 *
 * @return string
 */
function _ding_pretix_get_event_admin_url($service_settings, $event_slug) {
  $endpoint = $service_settings['service_url'] . 'control/event/' . $service_settings['organizer_slug'] . '/' . $event_slug . '/';

  return $endpoint;
}

/**
 * Get the http request headers.
 *
 * @param $service_settings
 *
 * @return array
 */
function _ding_pretix_built_request_headers($service_settings) {
  $headers = [
    'Accept' => 'application/json, text/javascript',
    'Authorization' => 'Token ' . $service_settings['api_key'],
    'Content-Type' => 'application/json',
  ];

  return $headers;
}

/**
 * Get the http request body for an event.
 *
 * Get the http request body for the event with values set from the node and
 * request type.
 *
 * @param $node
 * @param $request_type
 *
 * @return array
 *
 * @throws Exception
 */
function _ding_pretix_built_request_body($node, $request_type = 'UPDATE') {
  $wrapper = entity_metadata_wrapper('node', $node);

  $start_date = date_create('@' . $wrapper->field_ding_event_date->value->value());
  $end_date = date_create('@' . $wrapper->field_ding_event_date->value2->value());

  $field_address = _ding_pretix_get_place($node);
  $place = _ding_pretix_get_place_string($field_address);

  // Events cannot be created as 'live' in Pretix.
  $live = ($request_type === 'INSERT') ? FALSE : $node->status;

  $body = [
    'name' => ['da' => check_plain($node->title)],
    'slug' => $node->nid,
    'live' => $live,
    'currency' => 'DKK',
    'date_from' => date_format($start_date, 'c'),
    'date_to' => date_format($end_date, 'c'),
    'is_public' => $node->status,
    'location' => ['da' => $place],
    'has_subevents' => FALSE,
  ];

  $ding_pretix_psp_elements = variable_get('ding_pretix_psp_elements', []);
  if ($request_type === 'INSERT' && array_key_exists('pretix_psp_meta_key', $ding_pretix_psp_elements) && !empty($ding_pretix_psp_elements['pretix_psp_meta_key'])) {
    $key = $ding_pretix_psp_elements['pretix_psp_meta_key'];
    $body['meta_data'] = [$key => $node->pretix['psp_element']];
  }

  return $body;
}

/**
 * Get the place for the node.
 *
 * If 'name' is set on the address field on the node this field is
 * used. Otherwise the address field on the attached library is used.
 *
 * @param $node
 *
 * @return field|bool
 */
function _ding_pretix_get_place($node) {
  $field_address = field_get_items('node', $node, 'field_ding_event_location');

  // If the events has a name value we use the place set on the event. Else we
  // load the location of the first selected library.
  if (empty($field_address[0]['name_line'])) {
    $library_nid = _ding_pretix_get_library_nid($node);

    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', 'node')
      ->entityCondition('entity_id', $library_nid, '=')
      ->propertyCondition('status', NODE_PUBLISHED);
    $result = $query->execute();
    $libraries = $result['node'] ?? NULL;

    if ($libraries) {
      // Get id of location field.
      $fields = field_info_instances('node', 'ding_library');
      $field_id = $fields['field_ding_library_addresse']['field_id'];

      // Attach a field of selected id only to get value for it.
      field_attach_load('node', $libraries, FIELD_LOAD_CURRENT, ['field_id' => $field_id]);
      $library = $libraries[$library_nid];

      // Get the field.
      $field_address = field_get_items('node', $library, 'field_ding_library_addresse');
    }

  }

  return $field_address;
}

/**
 * Get a formatted string from the address field.
 *
 * @param $field_address
 *
 * @return string
 */
function _ding_pretix_get_place_string($field_address) {
  $place = '';
  $place = empty($field_address[0]['name_line']) ? $place : $place . $field_address[0]['name_line'] . "\n";
  $place = empty($field_address[0]['thoroughfare']) ? $place : $place . $field_address[0]['thoroughfare'] . "\n";
  $place = empty($field_address[0]['premise']) ? $place : $place . $field_address[0]['premise'] . "\n";
  $place = empty($field_address[0]['postal_code']) ? $place : $place . $field_address[0]['postal_code'] . "\n";
  $place = empty($field_address[0]['locality']) ? $place : $place . $field_address[0]['locality'] . "\n";
  $place = empty($field_address[0]['country']) ? $place : $place . $field_address[0]['country'] . "\n";

  return $place;
}

/**
 * Gets the library node id from event entity.
 *
 * @param $entity
 *   The entity to find library node id for.
 *
 * @return null|int
 *   If no library is selected on the entity NULL is returned else the
 *   entity id (nid) for the first library found.
 */
function _ding_pretix_get_library_nid($entity) {
  // Get the library id for the event using entity metadata wrappers.
  $node_wrapper = entity_metadata_wrapper('node', $entity);

  $libraries = $node_wrapper->og_group_ref->value();
  $library_nid = empty($libraries) ? NULL : $libraries[0]->nid;

  return $library_nid;
}

/**
 * Save pretix values for the given node.
 *
 * @param $node
 * @param $organizer
 * @param $data
 *
 * @return array
 *
 * @throws Exception
 */
function _ding_pretix_insert_pretix_node_info($node, $organizer, $data = NULL) {
  $nid = (int) $node->nid;
  $slug = $data ? json_decode($data)->slug : NULL;

  $pretix_info = [
    'nid' => $nid,
    'pretix_organizer' => $organizer,
    'pretix_slug' => $slug,
    'capacity' => (int) $node->pretix['capacity'],
    'maintain_copy' => $node->pretix['maintain_copy'],
    'psp_element' => $node->pretix['psp_element'],
    'ticket_form' => $node->pretix['ticket_form'],
  ];

  db_merge('ding_pretix')
    ->key(['nid' => $nid])
    ->fields($pretix_info)
    ->execute();

  return $pretix_info;
}

/**
 * Update the saved pretix values for the given node.
 *
 * @param $node
 *
 * @return mixed
 *
 * @throws InvalidMergeQueryException
 */
function _ding_pretix_update_pretix_node_info($node) {
  $nid = (int) $node->nid;

  if ($node->pretix) {
    $pretix_info = [
      'nid' => $nid,
      'capacity' => (int) $node->pretix['capacity'],
      'maintain_copy' => $node->pretix['maintain_copy'],
      'psp_element' => $node->pretix['psp_element'],
      'ticket_form' => $node->pretix['ticket_form'],
    ];

    db_merge('ding_pretix')
      ->key(['nid' => $nid])
      ->fields($pretix_info)
      ->execute();
  }

  return _ding_pretix_get_pretix_node_info($node->nid);
}

/**
 * Get the saved pretix values for the given node.
 *
 * @param $nid
 *
 * @return mixed
 */
function _ding_pretix_get_pretix_node_info($nid) {
  $pretix_info = db_select('ding_pretix', 'p')
    ->fields('p')
    ->condition('nid', $nid, '=')
    ->execute()
    ->fetchAssoc();

  return $pretix_info;
}

/**
 * Remove saved pretix values for the node.
 *
 * @param $nid
 *
 * @return int
 */
function _ding_pretix_delete_pretix_node_info($nid) {
  $pretix_info = db_delete('ding_pretix')
    ->condition('nid', $nid, '=')
    ->execute();

  return $pretix_info;
}

/**
 * Get the default values for pretix settings.
 *
 * @param $nid
 *
 * @return array
 */
function _ding_pretix_get_pretix_node_info_defaults($nid) {
  $settings = variable_get('ding_pretix_event_nodes', []);

  // If this is a new node we use the defaults. If it's an existing node we
  // don't automatically add it to Pretix.
  $maintain_copy = ($nid === NULL) ? $settings['defaults']['maintain_copy'] : FALSE;

  $pretix_defaults = [
    'capacity' => (int) $settings['capacity'],
    'maintain_copy' => $maintain_copy,
    'default_ticket_form' => $settings['default_ticket_form'],
  ];

  return $pretix_defaults;
}

/**
 * Perform http request.
 *
 * Uses 'drupal_http_request', logs all errors through watchdog.
 *
 * @param $item_endpoint
 * @param $options
 *
 * @return object
 */
function _ding_pretix_http_request($item_endpoint, $options) {
  $result = drupal_http_request($item_endpoint, $options);

  if (isset($result->error)) {
    watchdog('ding_pretix',
             '@error: @method @data (Request: @request, URL: @url)',
             [
               '@error' => $result->error,
               '@method' => $options['method'],
               '@data' => $result->data ?? NULL,
               '@request' => $result->request ?? NULL,
               '@url' => $item_endpoint,
             ],
             $severity = WATCHDOG_ERROR);
  }

  return $result;
}