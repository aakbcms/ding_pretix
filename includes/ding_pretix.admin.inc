<?php

/**
 * @file
 * Admin forms, page callbacks and related functions.
 */

/**
 * Administrative settings form.
 *
 * @param array $form
 *   Form structure.
 * @param array $form_state
 *   Form state values.
 *
 * @return array
 *   Form structure prepared for admin settings.
 *
 * @ingroup forms
 */
function ding_pretix_admin_settings_form($form, &$form_state) {
  global $user;

  $form['#tree'] = TRUE;

  $form['ding_pretix_ticketinfo_labels'] = [
    '#type' => 'item',
    '#title' => t('Labels for display of ticket information'),
    '#markup' => t('<p>5 texts exist for displaying ticket information:</p><ul><li>"The event has already taken place"</li><li>"The event is closed for admission"</li><li>"Ticket sale has not yet started for this event"</li><li>"Sold out"</li><li>"Book a ticket"</li></ul><p>These can all be translated to local needs - just paste the exact texts into the search box at <a href="/admin/config/regional/translate/translate">/admin/config/regional/translate/translate</a>.</p>'),
  ];

  // Load default settings.
  $defaults = variable_get('ding_pretix', []);

  // Default settings.
  $form['ding_pretix'] = [
    '#type' => 'fieldset',
    '#title' => t('Pretix service'),
    '#collapsible' => TRUE,
    '#collapsed' => isset($defaults['service_url']) ? TRUE : FALSE,
  ];

  $form['ding_pretix']['service_url'] = [
    '#type' => 'textfield',
    '#title' => t('Pretix service URL'),
    '#default_value' => array_key_exists('service_url', $defaults) ? $defaults['service_url'] : 'https://www.pretix.eu/',
    '#description' => t('Enter a valid Pretix service endpoint without path info, such as https://www.pretix.eu/'),
    '#required' => TRUE,
    '#size' => 100,
  ];

  $form['ding_pretix']['organizer_slug'] = [
    '#type' => 'textfield',
    '#title' => t('Pretix default organizer slug'),
    '#default_value' => array_key_exists('organizer_slug', $defaults) ? $defaults['organizer_slug'] : $user->name,
    '#required' => TRUE,
    '#description' => t('This is the default organizer slug used when connecting to Pretix. If you provide slug/API key for a specific library (below), events related to that library will use that key instead of the default key.'),
  ];

  $form['ding_pretix']['api_key'] = [
    '#type' => 'textfield',
    '#title' => t('The API key of the Organizer Team'),
    '#default_value' => array_key_exists('api_key', $defaults) ? $defaults['api_key'] : '',
    '#required' => TRUE,
    '#description' => t('This is the default API key used when connecting to Pretix. If you provide slug/API key for a specific library (below), events related to that library will use that key instead of the default key.'),
  ];

  $form['ding_pretix']['template_event_slug'] = [
    '#type' => 'textfield',
    '#title' => t('The slug of the default event template'),
    '#default_value' => array_key_exists('template_event_slug', $defaults) ? $defaults['template_event_slug'] : '',
    '#required' => TRUE,
    '#description' => t('This is the slug of the default event template. When events are created their setting etc. are copied from this event.'),
  ];

  $form['ding_pretix_libraries'] = [
    '#type' => 'fieldset',
    '#title' => t('Individual library slug/API keys'),
    '#description' => t('Optional. If you have several organizers at Pretix, each library can have their own slug/API key. In that case, the base slug/API key will be overridden by the provided key when sending data on events related to this library.'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  ];

  // Load default settings.
  $defaults = variable_get('ding_pretix_libraries', []);

  // Find all libraries and order them by title.
  $query = new EntityFieldQuery();
  $query->entityCondition('entity_type', 'node')
    ->entityCondition('bundle', 'ding_library')
    ->propertyOrderBy('title', 'fid', 'DESC')
    ->addMetaData('account', user_load(1));
  $result = $query->execute();

  if (isset($result['node'])) {
    $libraries_nids = array_keys($result['node']);
    $libraries = entity_load('node', $libraries_nids);
  }

  foreach ($libraries as $library) {
    $form['ding_pretix_libraries'][$library->nid] = [
      '#type' => 'fieldset',
      '#title' => t('@library API keys', ['@library' => $library->title]),
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
    ];

    $form['ding_pretix_libraries'][$library->nid]['slug'] = [
      '#type' => 'textfield',
      '#title' => t('Slug @library', ['@library' => $library->title]),
      '#default_value' => array_key_exists($library->nid, $defaults) ? $defaults[$library->nid]['slug'] : '',
      '#description' => t('The slug of the Pretix organizer to map to.'),
    ];

    $form['ding_pretix_libraries'][$library->nid]['apikey'] = [
      '#type' => 'textfield',
      '#title' => t('API key @library', ['@library' => $library->title]),
      '#default_value' => array_key_exists($library->nid, $defaults) ? $defaults[$library->nid]['apikey'] : '',
      '#description' => t('The API key of the Organizer Team'),
    ];
  }

  // Get PSPs previously saved to a variable (first load), else use formstate
  // data (ajax calls).
  $ding_pretix_psp_elements = variable_get('ding_pretix_psp_elements', []);
  $psp_elements = $form_state['values']['ding_pretix_psp_elements']['list'] ?? $ding_pretix_psp_elements['list'];

  $form['ding_pretix_psp_elements'] = [
    '#type' => 'fieldset',
    '#title' => t('PSP elements'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  ];

  $form['ding_pretix_psp_elements']['pretix_psp_meta_key'] = [
    '#type' => 'textfield',
    '#title' => t('Pretix PSP property name'),
    '#default_value' => array_key_exists('pretix_psp_meta_key', $ding_pretix_psp_elements) ? $ding_pretix_psp_elements['pretix_psp_meta_key'] : '',
    '#size' => 50,
    '#maxlength' => 50,
    '#description' => t('The name of the organizer metadata property for the PSP element in Pretix (case sensitive).'),
  ];

  $form['ding_pretix_psp_elements']['list'] = [
    '#prefix' => '<div id="ding-pretix-psp-elements-list">',
    '#suffix' => '</div>',
  ];

  foreach ($psp_elements as $key => $value) {
    $form['ding_pretix_psp_elements']['list'][$key] = [
      '#type' => 'fieldset',
      '#title' => $key ? t('PSP element') : t('PSP element (default)'),
      '#collapsible' => FALSE,
      '#collapsed' => FALSE,
    ];
    $form['ding_pretix_psp_elements']['list'][$key]['psp_name'] = [
      '#type' => 'textfield',
      '#title' => t('Name'),
      '#default_value' => $value['psp_name'],
    ];
    $form['ding_pretix_psp_elements']['list'][$key]['psp_value'] = [
      '#type' => 'textfield',
      '#title' => t('Value'),
      '#size' => 50,
      '#maxlength' => 50,
      '#default_value' => $value['psp_value'],
    ];
  }

  $form['ding_pretix_psp_elements']['add_psp'] = [
    '#type' => 'submit',
    '#value' => t('Add PSP element'),
    '#submit' => ['ding_pretix_admin_settings_form_add_psp_element'],
    '#ajax' => [
      'callback' => 'ding_pretix_admin_settings_form_psp_callback',
      'wrapper' => 'ding-pretix-psp-elements-list',
    ],
  ];

  $form['ding_pretix_psp_elements']['remove_psp'] = [
    '#type' => 'submit',
    '#value' => t('Remove PSP element'),
    '#submit' => ['ding_pretix_admin_settings_form_remove_psp_element'],
    '#ajax' => [
      'callback' => 'ding_pretix_admin_settings_form_psp_callback',
      'wrapper' => 'ding-pretix-psp-elements-list',
    ],
  ];

  // Load default event node pretix settings.
  $defaults = variable_get('ding_pretix_event_nodes', []);

  $form['ding_pretix_event_nodes'] = [
    '#type' => 'fieldset',
    '#title' => t('Pretix event node defaults'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  ];

  $form['ding_pretix_event_nodes']['capacity'] = [
    '#type' => 'textfield',
    '#title' => t('Capacity'),
    '#default_value' => array_key_exists('capacity', $defaults) ? $defaults['capacity'] : 0,
    '#size' => 5,
    '#maxlength' => 5,
    '#description' => t('The default capacity for new events. Set to 0 for unlimited capacity.'),
  ];

  $form['ding_pretix_event_nodes']['defaults'] = [
    '#type' => 'checkboxes',
    '#title' => t('Maintain copy in Pretix'),
    '#options' => [
      'maintain_copy' => t('Maintain copy'),
    ],
    '#default_value' => array_key_exists('defaults', $defaults) ? $defaults['defaults'] : [],
    '#description' => t('Should new events be saved and updated to Pretix by default?'),
  ];

  $form['ding_pretix_event_nodes']['default_ticket_form'] = [
    '#type' => 'radios',
    '#title' => t('Use PDF or Email tickets'),
    '#options' => [
      'pdf_ticket' => t('PDF Tickets'),
      'email_ticket' => t('Email Tickets'),
    ],
    '#required' => TRUE,
    '#default_value' => array_key_exists('default_ticket_form', $defaults) ? $defaults['default_ticket_form'] : [],
    '#description' => t('Should new events use PDF or Email tickets by default?'),
  ];

  return system_settings_form($form);
}

/**
 * Callback for both ajax-enabled buttons.
 *
 * Selects and returns the fieldset with the PSP elements in it.
 *
 * @param array $form
 *   Form structure.
 * @param array $form_state
 *   Form state values.
 *
 * @return array
 *   Fieldset of PSP elements.
 */
function ding_pretix_admin_settings_form_psp_callback($form, $form_state) {
  return $form['ding_pretix_psp_elements']['list'];
}

/**
 * Submit handler for the "add-one-more" button.
 *
 * Increments the max counter and causes a rebuild.
 *
 * @param array $form
 *   Form structure.
 * @param array $form_state
 *   Form state values.
 */
function ding_pretix_admin_settings_form_add_psp_element($form, &$form_state) {
  $form_state['values']['ding_pretix_psp_elements']['list'][] = ['psp_name' => '', 'psp_value' => ''];

  $form_state['rebuild'] = TRUE;
}

/**
 * Submit handler for the "remove one" button.
 *
 * Decrements the max counter and causes a form rebuild.
 *
 * @param array $form
 *   Form structure.
 * @param array $form_state
 *   Form state values.
 */
function ding_pretix_admin_settings_form_remove_psp_element($form, &$form_state) {
  array_pop($form_state['values']['ding_pretix_psp_elements']['list']);

  $form_state['rebuild'] = TRUE;
}
